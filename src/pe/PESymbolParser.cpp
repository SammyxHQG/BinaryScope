#include "PESymbolParser.h"
#include "core/ByteReader.h"
#include <algorithm>
#include <limits>
namespace bs {
namespace {
constexpr std::uint32_t symbolLimit = 500000;
class RvaReader {
  public:
    RvaReader(const PEImage& pe, std::span<const std::uint8_t> bytes) : pe_(pe), reader_(bytes) {}
    std::uint64_t offset(std::uint64_t rva, std::uint64_t length) const {
        if (rva > 0xffffffffULL || length > 0x100000000ULL - rva)
            throw ParseError("Symbol RVA range overflows.");
        const auto mapped = pe_.rvaToOffset(rva, length);
        if (!mapped)
            throw ParseError("Symbol table references an RVA without contiguous file data.");
        reader_.require(*mapped, length);
        return *mapped;
    }
    template <class T> T read(std::uint64_t rva) const { return reader_.read<T>(offset(rva, sizeof(T))); }
    std::string string(std::uint64_t rva, std::uint64_t end = 0x100000000ULL) const {
        std::string result;
        for (unsigned i = 0; i < 4096 && rva + i < end; ++i) {
            const auto c = read<std::uint8_t>(rva + i);
            if (!c)
                return result;
            if (++stringBytes_ > 32 * 1024 * 1024)
                throw ParseError("Symbol text exceeds the 32 MiB safety budget.");
            result.push_back(static_cast<char>(c));
        }
        throw ParseError("Symbol name is unterminated or exceeds the 4096-byte limit.");
    }

  private:
    const PEImage& pe_;
    ByteReader reader_;
    mutable std::size_t stringBytes_{};
};
} // namespace
void parseImports(PEImage& pe, std::span<const std::uint8_t> bytes) {
    if (pe.directories.size() <= 1 || !pe.directories[1].rva || !pe.directories[1].size)
        return;
    const auto d = pe.directories[1];
    RvaReader r(pe, bytes);
    r.offset(d.rva, d.size);
    bool terminated = false;
    std::size_t storedText = 0;
    for (std::uint64_t pos = 0; pos + 20 <= d.size; pos += 20) {
        if (pos / 20 >= 4096)
            throw ParseError("Import descriptor count exceeds the 4096-entry safety limit.");
        const auto at = std::uint64_t(d.rva) + pos;
        const auto lookup = r.read<std::uint32_t>(at), stamp = r.read<std::uint32_t>(at + 4),
                   chain = r.read<std::uint32_t>(at + 8);
        const auto name = r.read<std::uint32_t>(at + 12), iat = r.read<std::uint32_t>(at + 16);
        if (!(lookup | stamp | chain | name | iat)) {
            terminated = true;
            break;
        }
        if (!name || !iat)
            throw ParseError("Import descriptor lacks DLL name or IAT address.");
        const auto dll = r.string(name);
        const auto thunk = lookup ? lookup : iat;
        const unsigned width = pe.is64 ? 8 : 4;
        for (std::uint64_t i = 0;; ++i) {
            if (i >= symbolLimit || pe.imports.size() >= symbolLimit)
                throw ParseError("Import count exceeds the 500000-symbol safety limit.");
            const auto value = pe.is64 ? r.read<std::uint64_t>(thunk + i * width)
                                       : std::uint64_t(r.read<std::uint32_t>(thunk + i * width));
            if (!value)
                break;
            const auto iatRva = std::uint64_t(iat) + i * width;
            r.offset(iatRva, width);
            PEImport symbol{dll, {}, {}, static_cast<std::uint32_t>(iatRva)};
            const auto ordinalBit = pe.is64 ? 0x8000000000000000ULL : 0x80000000ULL;
            if (value & ordinalBit) {
                if (value & ~(ordinalBit | 0xffffULL))
                    throw ParseError("Import ordinal contains reserved bits.");
                symbol.ordinal = static_cast<std::uint16_t>(value & 0xffff);
            } else {
                if (value > 0xffffffffULL)
                    throw ParseError(
                        "Import name RVA exceeds 32 bits (possibly a bound IAT without lookup table).");
                r.read<std::uint16_t>(value);
                symbol.name = r.string(value + 2);
            }
            storedText += symbol.dll.size() + symbol.name.size();
            if (storedText > 32 * 1024 * 1024)
                throw ParseError("Import text exceeds the 32 MiB safety budget.");
            pe.imports.push_back(std::move(symbol));
        }
    }
    if (!terminated)
        throw ParseError("Import directory has no terminating descriptor within its declared size.");
}
void parseExports(PEImage& pe, std::span<const std::uint8_t> bytes) {
    if (pe.directories.empty() || !pe.directories[0].rva || !pe.directories[0].size)
        return;
    const auto d = pe.directories[0];
    RvaReader r(pe, bytes);
    if (d.size < 40)
        throw ParseError("Truncated export directory.");
    r.offset(d.rva, d.size);
    const std::uint64_t at = d.rva;
    const auto base = r.read<std::uint32_t>(at + 16), count = r.read<std::uint32_t>(at + 20),
               names = r.read<std::uint32_t>(at + 24);
    const auto functions = r.read<std::uint32_t>(at + 28), nameTable = r.read<std::uint32_t>(at + 32),
               ordinals = r.read<std::uint32_t>(at + 36);
    if (count > symbolLimit || names > symbolLimit)
        throw ParseError("Export count exceeds the 500000-symbol safety limit.");
    if (count && std::uint64_t(base) + count - 1 > 0xffffffffULL)
        throw ParseError("Export ordinal range overflows.");
    if (count)
        r.offset(functions, std::uint64_t(count) * 4);
    if (names) {
        r.offset(nameTable, std::uint64_t(names) * 4);
        r.offset(ordinals, std::uint64_t(names) * 2);
    }
    std::vector<std::vector<std::string>> labels(count);
    for (std::uint64_t i = 0; i < names; ++i) {
        const auto index = r.read<std::uint16_t>(ordinals + i * 2);
        if (index >= count)
            throw ParseError("Export name ordinal is outside the address table.");
        labels[index].push_back(r.string(r.read<std::uint32_t>(nameTable + i * 4)));
    }
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto rva = r.read<std::uint32_t>(functions + i * 4);
        if (!rva)
            continue; // Holes are not exported functions.
        std::string forwarder;
        if (rva >= d.rva && std::uint64_t(rva) < std::uint64_t(d.rva) + d.size)
            forwarder = r.string(rva, std::uint64_t(d.rva) + d.size);
        if (rva >= pe.sizeOfImage)
            throw ParseError("Export RVA exceeds SizeOfImage.");
        if (labels[i].empty())
            labels[i].push_back({});
        for (auto& label : labels[i])
            pe.exports.push_back({std::move(label), forwarder, static_cast<std::uint32_t>(base + i), rva});
    }
}
} // namespace bs
