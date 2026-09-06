#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bs {
struct HeaderField {
    std::string group, name;
    std::uint64_t value;
    std::string description;
};
struct DataDirectory {
    std::uint32_t rva{}, size{};
};
struct PESection {
    std::string name;
    std::uint32_t virtualAddress{}, virtualSize{}, rawOffset{}, rawSize{}, characteristics{};
    double entropy{};
};
struct PEImport {
    std::string dll, name;
    std::optional<std::uint16_t> ordinal;
    std::uint32_t iatRva{};
};
struct PEExport {
    std::string name, forwarder;
    std::uint32_t ordinal{}, rva{};
};
struct PEImage {
    bool is64{};
    std::uint16_t machine{}, characteristics{}, subsystem{};
    std::uint32_t timestamp{}, entryRva{}, sizeOfHeaders{}, sizeOfImage{};
    std::uint64_t imageBase{};
    std::vector<HeaderField> fields;
    std::vector<DataDirectory> directories;
    std::vector<PESection> sections;
    std::vector<PEImport> imports;
    std::vector<PEExport> exports;
    std::vector<std::string> warnings;
    [[nodiscard]] std::optional<std::uint64_t> rvaToOffset(std::uint64_t rva, std::uint64_t length = 1) const;
    [[nodiscard]] std::string architecture() const;
    [[nodiscard]] std::string subsystemName() const;
};
} // namespace bs
