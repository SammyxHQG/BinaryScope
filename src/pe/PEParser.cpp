#include "PEParser.h"
#include "PESymbolParser.h"
#include "core/ByteReader.h"
#include <algorithm>
#include <limits>

namespace bs {
std::optional<std::uint64_t> PEImage::rvaToOffset(std::uint64_t rva, std::uint64_t length) const {
    if (rva < sizeOfHeaders && length <= sizeOfHeaders - rva)
        return rva;
    for (const auto& s : sections) {
        if (rva < s.virtualAddress)
            continue;
        const auto delta = rva - s.virtualAddress;
        if (delta < s.rawSize && length <= s.rawSize - delta)
            return std::uint64_t(s.rawOffset) + delta;
    }
    return std::nullopt; // Virtual zero-fill is not present in the file.
}
std::string PEImage::architecture() const {
    switch (machine) {
    case 0x14c:
        return "x86";
    case 0x8664:
        return "x86-64";
    case 0xaa64:
        return "ARM64";
    case 0x1c4:
        return "ARM Thumb-2";
    default:
        return "Unknown architecture";
    }
}
std::string PEImage::subsystemName() const {
    switch (subsystem) {
    case 1:
        return "Native";
    case 2:
        return "Windows GUI";
    case 3:
        return "Windows console";
    case 9:
        return "Windows CE";
    case 10:
        return "EFI application";
    case 11:
        return "EFI boot service driver";
    case 12:
        return "EFI runtime driver";
    case 14:
        return "Xbox";
    case 16:
        return "Windows boot application";
    default:
        return "Unknown (" + std::to_string(subsystem) + ")";
    }
}
PEImage PEParser::parse(std::span<const std::uint8_t> bytes) {
    ByteReader r(bytes);
    if (bytes.size() < 64)
        throw ParseError("Truncated DOS header (at least 64 bytes required).");
    if (r.read<std::uint16_t>(0) != 0x5a4d)
        throw ParseError("Invalid DOS header: expected MZ signature.");
    const std::uint64_t nt = r.read<std::uint32_t>(0x3c);
    if (nt < 64)
        throw ParseError("Invalid e_lfanew: PE header overlaps the DOS header.");
    r.require(nt, 24);
    if (r.read<std::uint32_t>(nt) != 0x4550)
        throw ParseError("Invalid PE signature: expected PE\\0\\0.");
    PEImage pe;
    auto field = [&](std::string group, std::string name, std::uint64_t offset, unsigned width,
                     std::string description) {
        const auto value = width == 1   ? std::uint64_t(r.read<std::uint8_t>(offset))
                           : width == 2 ? std::uint64_t(r.read<std::uint16_t>(offset))
                           : width == 4 ? std::uint64_t(r.read<std::uint32_t>(offset))
                                        : r.read<std::uint64_t>(offset);
        pe.fields.push_back({std::move(group), std::move(name), value, std::move(description)});
        return value;
    };
    field("DOS Header", "e_magic", 0, 2, "DOS signature (MZ)");
    const std::vector<std::pair<std::string, std::string>> dosFields{
        {"e_cblp", "Bytes used in the last DOS page"},
        {"e_cp", "DOS page count"},
        {"e_crlc", "DOS relocation count"},
        {"e_cparhdr", "DOS header size in paragraphs"},
        {"e_minalloc", "Minimum extra paragraphs"},
        {"e_maxalloc", "Maximum extra paragraphs"},
        {"e_ss", "Initial DOS stack segment"},
        {"e_sp", "Initial DOS stack pointer"},
        {"e_csum", "DOS checksum"},
        {"e_ip", "Initial DOS instruction pointer"},
        {"e_cs", "Initial DOS code segment"},
        {"e_lfarlc", "DOS relocation table offset"},
        {"e_ovno", "Overlay number"}};
    for (std::size_t i = 0; i < dosFields.size(); ++i)
        field("DOS Header", dosFields[i].first, 2 + i * 2, 2, dosFields[i].second);
    field("DOS Header", "e_oemid", 0x24, 2, "OEM identifier");
    field("DOS Header", "e_oeminfo", 0x26, 2, "OEM-specific information");
    field("DOS Header", "e_lfanew", 0x3c, 4, "File offset of the NT headers");
    field("NT Headers", "Signature", nt, 4, "PE signature");
    pe.machine = r.read<std::uint16_t>(nt + 4);
    const auto count = r.read<std::uint16_t>(nt + 6);
    pe.timestamp = r.read<std::uint32_t>(nt + 8);
    const auto optionalSize = r.read<std::uint16_t>(nt + 20);
    pe.characteristics = r.read<std::uint16_t>(nt + 22);
    field("File Header", "Machine", nt + 4, 2, "Target instruction set");
    field("File Header", "NumberOfSections", nt + 6, 2, "Number of section table records");
    field("File Header", "TimeDateStamp", nt + 8, 4, "COFF timestamp; may be reproducible-build metadata");
    field("File Header", "PointerToSymbolTable", nt + 12, 4, "COFF symbol table file offset");
    field("File Header", "NumberOfSymbols", nt + 16, 4, "COFF symbol record count");
    field("File Header", "SizeOfOptionalHeader", nt + 20, 2, "Optional header byte length");
    field("File Header", "Characteristics", nt + 22, 2, "Image attributes; 0x2000 denotes DLL");
    const auto opt = nt + 24;
    r.require(opt, optionalSize);
    if (optionalSize < 2)
        throw ParseError("Missing PE optional header.");
    const auto magic = r.read<std::uint16_t>(opt);
    if (magic != 0x10b && magic != 0x20b)
        throw ParseError("Unsupported optional header magic (expected PE32 or PE32+).");
    pe.is64 = magic == 0x20b;
    if ((pe.machine == 0x14c && pe.is64) || ((pe.machine == 0x8664 || pe.machine == 0xaa64) && !pe.is64))
        throw ParseError("Machine architecture and PE optional header type disagree.");
    const unsigned fixedSize = pe.is64 ? 112 : 96;
    if (optionalSize < fixedSize)
        throw ParseError("Truncated PE optional header.");
    pe.entryRva = r.read<std::uint32_t>(opt + 16);
    pe.imageBase = pe.is64 ? r.read<std::uint64_t>(opt + 24) : r.read<std::uint32_t>(opt + 28);
    pe.sizeOfImage = r.read<std::uint32_t>(opt + 56);
    pe.sizeOfHeaders = r.read<std::uint32_t>(opt + 60);
    pe.subsystem = r.read<std::uint16_t>(opt + 68);
    if (pe.imageBase > std::numeric_limits<std::uint64_t>::max() - std::max(pe.sizeOfImage, pe.entryRva))
        throw ParseError("Image virtual address range overflows 64 bits.");
    field("Optional Header", "Magic", opt, 2, "0x10B: PE32; 0x20B: PE32+");
    field("Optional Header", "MajorLinkerVersion", opt + 2, 1, "Linker major version");
    field("Optional Header", "MinorLinkerVersion", opt + 3, 1, "Linker minor version");
    for (const auto& f : std::vector<std::pair<unsigned, std::string>>{{4, "SizeOfCode"},
                                                                       {8, "SizeOfInitializedData"},
                                                                       {12, "SizeOfUninitializedData"},
                                                                       {16, "AddressOfEntryPoint"},
                                                                       {20, "BaseOfCode"},
                                                                       {32, "SectionAlignment"},
                                                                       {36, "FileAlignment"},
                                                                       {56, "SizeOfImage"},
                                                                       {60, "SizeOfHeaders"},
                                                                       {64, "CheckSum"}})
        field("Optional Header", f.second, opt + f.first, 4,
              f.first == 16 || f.first == 20 ? "Relative virtual address (RVA)" : "PE optional header value");
    field("Optional Header", "ImageBase", opt + (pe.is64 ? 24 : 28), pe.is64 ? 8 : 4,
          "Preferred load address");
    field("Optional Header", "Subsystem", opt + 68, 2, "Required execution environment");
    field("Optional Header", "DllCharacteristics", opt + 70, 2, "ASLR, DEP and other image flags");
    if (!pe.is64)
        field("Optional Header", "BaseOfData", opt + 24, 4, "RVA of the data section");
    for (const auto& f : std::vector<std::pair<unsigned, std::string>>{{40, "MajorOperatingSystemVersion"},
                                                                       {42, "MinorOperatingSystemVersion"},
                                                                       {44, "MajorImageVersion"},
                                                                       {46, "MinorImageVersion"},
                                                                       {48, "MajorSubsystemVersion"},
                                                                       {50, "MinorSubsystemVersion"}})
        field("Optional Header", f.second, opt + f.first, 2, "Version component declared by the image");
    field("Optional Header", "Win32VersionValue", opt + 52, 4, "Reserved; should be zero");
    const unsigned pointerWidth = pe.is64 ? 8 : 4;
    const std::vector<std::string> memoryFields{"SizeOfStackReserve", "SizeOfStackCommit",
                                                "SizeOfHeapReserve", "SizeOfHeapCommit"};
    for (unsigned i = 0; i < 4; ++i)
        field("Optional Header", memoryFields[i], opt + 72 + i * pointerWidth, pointerWidth,
              "Reserved or committed memory size in bytes");
    field("Optional Header", "LoaderFlags", opt + 72 + 4 * pointerWidth, 4, "Reserved; should be zero");
    field("Optional Header", "NumberOfRvaAndSizes", opt + fixedSize - 4, 4,
          "Number of data directory entries");
    const auto dirCount = r.read<std::uint32_t>(opt + fixedSize - 4);
    if (dirCount > (optionalSize - fixedSize) / 8)
        throw ParseError("Data directories exceed the optional header.");
    for (unsigned i = 0; i < dirCount; ++i)
        pe.directories.push_back({r.read<std::uint32_t>(opt + fixedSize + i * 8),
                                  r.read<std::uint32_t>(opt + fixedSize + i * 8 + 4)});
    if (count == 0 || count > 96)
        throw ParseError("Invalid section count (Windows images support 1-96 sections).");
    const auto table = opt + optionalSize;
    r.require(table, std::uint64_t(count) * 40);
    if (pe.sizeOfHeaders < table + std::uint64_t(count) * 40 || pe.sizeOfHeaders > bytes.size())
        throw ParseError(
            "Invalid SizeOfHeaders: headers must include the section table and fit in the file.");
    for (unsigned i = 0; i < count; ++i) {
        const auto s = table + i * 40;
        PESection section{r.fixedString(s, 8),           r.read<std::uint32_t>(s + 12),
                          r.read<std::uint32_t>(s + 8),  r.read<std::uint32_t>(s + 20),
                          r.read<std::uint32_t>(s + 16), r.read<std::uint32_t>(s + 36)};
        if (section.rawSize) {
            r.require(section.rawOffset, section.rawSize);
            if (section.rawOffset < pe.sizeOfHeaders)
                throw ParseError("Section raw data overlaps PE headers.");
        }
        const auto extent = std::max(section.virtualSize, section.rawSize);
        if (extent && section.virtualAddress < pe.sizeOfHeaders)
            throw ParseError("Section virtual data overlaps the mapped PE headers.");
        if (std::uint64_t(section.virtualAddress) + extent > pe.sizeOfImage)
            throw ParseError("Section virtual range exceeds SizeOfImage.");
        for (const auto& previous : pe.sections) {
            const bool rawOverlap =
                section.rawSize && previous.rawSize &&
                std::uint64_t(section.rawOffset) < std::uint64_t(previous.rawOffset) + previous.rawSize &&
                std::uint64_t(previous.rawOffset) < std::uint64_t(section.rawOffset) + section.rawSize;
            const bool virtualOverlap =
                extent && std::max(previous.virtualSize, previous.rawSize) &&
                std::uint64_t(section.virtualAddress) <
                    std::uint64_t(previous.virtualAddress) +
                        std::max(previous.virtualSize, previous.rawSize) &&
                std::uint64_t(previous.virtualAddress) < std::uint64_t(section.virtualAddress) + extent;
            if (rawOverlap || virtualOverlap)
                throw ParseError("Overlapping sections make address mapping ambiguous.");
        }
        pe.sections.push_back(std::move(section));
    }
    if (pe.machine != 0x14c && pe.machine != 0x8664 && pe.machine != 0xaa64 && pe.machine != 0x1c4)
        pe.warnings.push_back(
            "Unsupported architecture: structural inspection is available, disassembly is disabled.");
    if (pe.entryRva && !pe.rvaToOffset(pe.entryRva))
        pe.warnings.push_back("Entry point is not backed by file data.");
    // Keep useful structural results if an optional symbol directory is damaged.
    try {
        parseImports(pe, bytes);
    } catch (const ParseError& e) {
        pe.imports.clear();
        pe.warnings.push_back(std::string("Imports unavailable: ") + e.what());
    }
    try {
        parseExports(pe, bytes);
    } catch (const ParseError& e) {
        pe.exports.clear();
        pe.warnings.push_back(std::string("Exports unavailable: ") + e.what());
    }
    return pe;
}
} // namespace bs
