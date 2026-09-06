#include "StringExtractor.h"
#include <algorithm>
namespace bs {
namespace {
bool printableAscii(std::uint8_t c) {
    return c >= 0x20 && c <= 0x7e;
}
// Conservative Unicode text ranges: excludes controls, surrogates, private use and noncharacters.
bool printableUnicode(std::uint16_t c) {
    if ((c >= 0x200b && c <= 0x200f) || (c >= 0x2028 && c <= 0x202e) || (c >= 0x2060 && c <= 0x206f) ||
        c == 0xad || c == 0x061c)
        return false;
    return (c >= 0x20 && c <= 0x7e) || (c >= 0xa0 && c <= 0x2fff && c != 0x2028 && c != 0x2029) ||
           (c >= 0x3040 && c <= 0xd7a3) || (c >= 0xf900 && c <= 0xfaff) || (c >= 0xff01 && c <= 0xffef);
}
} // namespace
StringIndex StringExtractor::extract(std::span<const std::uint8_t> bytes, unsigned minimumLength,
                                     const std::atomic_bool* cancel) {
    StringIndex result;
    minimumLength = std::max(2U, minimumLength);
    const auto cancelled = [&] { return cancel && cancel->load(std::memory_order_relaxed); };
    constexpr std::size_t limit = 1000000;
    auto append = [&](std::size_t start, std::size_t length, StringEncoding encoding) {
        if (length >= minimumLength) {
            if (result.entries.size() == limit) {
                result.limited = true;
                return false;
            }
            result.entries.push_back({start, static_cast<std::uint32_t>(length), encoding});
        }
        return true;
    };
    for (std::size_t i = 0; i < bytes.size();) {
        if ((i & 0xffff) == 0 && cancelled())
            return {};
        if (!printableAscii(bytes[i])) {
            ++i;
            continue;
        }
        const auto start = i;
        while (i < bytes.size() && printableAscii(bytes[i])) {
            if ((i & 0xffff) == 0 && cancelled())
                return {};
            ++i;
        }
        if (!append(start, i - start, StringEncoding::Ascii))
            break;
    }
    // Examine both alignments. Require a terminator for wide strings to reduce random-byte noise.
    for (std::size_t alignment = 0; alignment < 2 && !result.limited; ++alignment) {
        for (std::size_t i = alignment; i + 1 < bytes.size();) {
            if (((i - alignment) & 0xffff) == 0 && cancelled())
                return {};
            auto unit = [&](std::size_t at) {
                return std::uint16_t(bytes[at] | (std::uint16_t(bytes[at + 1]) << 8));
            };
            if (!printableUnicode(unit(i))) {
                i += 2;
                continue;
            }
            const auto start = i;
            while (i + 1 < bytes.size() && printableUnicode(unit(i))) {
                if (((i - alignment) & 0xffff) == 0 && cancelled())
                    return {};
                i += 2;
            }
            if (i + 1 < bytes.size() && unit(i) == 0 &&
                !append(start, (i - start) / 2, StringEncoding::Utf16LE))
                break;
        }
    }
    if (cancelled())
        return {};
    std::sort(result.entries.begin(), result.entries.end(),
              [](const auto& a, const auto& b) { return a.offset < b.offset; });
    return result;
}
} // namespace bs
