#include "analysis/ByteSearch.h"
#include "analysis/Disassembler.h"
#include "analysis/EntropyAnalyzer.h"
#include "analysis/StringExtractor.h"
#include "core/BinaryFile.h"
#include "core/ByteReader.h"
#include "pe/PEParser.h"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
using Bytes = std::vector<std::uint8_t>;
void put(Bytes& b, std::size_t at, std::uint64_t value, int width) {
    for (int i = 0; i < width; ++i)
        b.at(at + i) = std::uint8_t(value >> (8 * i));
}
Bytes fixture(bool is64 = true) {
    Bytes b(1024);
    put(b, 0, 0x5a4d, 2);
    put(b, 0x3c, 0x80, 4);
    put(b, 0x80, 0x4550, 4);
    put(b, 0x84, is64 ? 0x8664 : 0x14c, 2);
    put(b, 0x86, 1, 2);
    put(b, 0x94, is64 ? 240 : 224, 2);
    put(b, 0x96, 0x22, 2);
    const std::size_t o = 0x98;
    put(b, o, is64 ? 0x20b : 0x10b, 2);
    put(b, o + 16, 0x1000, 4);
    put(b, o + (is64 ? 24 : 28), is64 ? 0x140000000ULL : 0x400000, is64 ? 8 : 4);
    put(b, o + 32, 0x1000, 4);
    put(b, o + 36, 0x200, 4);
    put(b, o + 56, 0x2000, 4);
    put(b, o + 60, 0x200, 4);
    put(b, o + 68, 3, 2);
    put(b, o + (is64 ? 108 : 92), 16, 4);
    const auto s = o + (is64 ? 240 : 224);
    b[s] = '.';
    b[s + 1] = 't';
    b[s + 2] = 'e';
    b[s + 3] = 'x';
    b[s + 4] = 't';
    put(b, s + 8, 0x300, 4);
    put(b, s + 12, 0x1000, 4);
    put(b, s + 16, 0x200, 4);
    put(b, s + 20, 0x200, 4);
    put(b, s + 36, 0x60000020, 4);
    b[0x200] = 0xc3;
    return b;
}
void check(bool ok, const char* message) {
    if (!ok)
        throw std::runtime_error(message);
}
void rejects(const Bytes& b) {
    bool rejected = false;
    try {
        (void)bs::PEParser::parse(b);
    } catch (const bs::ParseError&) {
        rejected = true;
    }
    check(rejected, "Malformed fixture was accepted");
}
int main(int argc, char* argv[]) {
    try {
        for (bool is64 : {false, true}) {
            const auto pe = bs::PEParser::parse(fixture(is64));
            check(pe.is64 == is64, "PE type");
            check(pe.rvaToOffset(0x1000) == 0x200, "RVA mapping");
            check(!pe.rvaToOffset(0x1200), "zero fill must not map");
            check(!pe.rvaToOffset(0x11ff, 2), "range crossing section");
        }
        auto good = fixture();
        for (std::size_t i = 0; i < good.size(); ++i)
            rejects(Bytes(good.begin(), good.begin() + i));
        auto b = good;
        put(b, 0x3c, 0xffffffff, 4);
        rejects(b);
        b = good;
        b[0] = 0;
        rejects(b);
        b = good;
        b[0x80] = 0;
        rejects(b);
        b = good;
        put(b, 0x98 + 108, 100, 4);
        rejects(b);
        b = good;
        put(b, 0x98 + 240 + 20, 0xfffffff0, 4);
        rejects(b);
        b = good;
        put(b, 0x86, 97, 2);
        rejects(b);
        // One descriptor with a name import and an ordinal import, then terminators.
        for (bool is64 : {false, true}) {
            b = fixture(is64);
            const auto dirs = 0x98 + (is64 ? 112 : 96);
            put(b, dirs + 8, 0x1020, 4);
            put(b, dirs + 12, 40, 4);
            put(b, 0x220, 0x1060, 4);
            put(b, 0x22c, 0x1050, 4);
            put(b, 0x230, 0x1060, 4);
            const std::string dll = "TEST.dll";
            std::copy(dll.begin(), dll.end(), b.begin() + 0x250);
            put(b, 0x260, 0x1090, is64 ? 8 : 4);
            put(b, 0x260 + (is64 ? 8 : 4), (is64 ? 0x8000000000000000ULL : 0x80000000ULL) | 42, is64 ? 8 : 4);
            const std::string name = "ReadThing";
            std::copy(name.begin(), name.end(), b.begin() + 0x292);
            const auto pe = bs::PEParser::parse(b);
            check(pe.imports.size() == 2, "Import count");
            check(pe.imports[0].name == name, "Name import");
            check(pe.imports[1].ordinal == 42, "Ordinal import");
            put(b, 0x22c, 0x5000, 4);
            const auto damaged = bs::PEParser::parse(b);
            check(damaged.imports.empty() && !damaged.warnings.empty(), "Bad imports must produce warning");
        }
        b = fixture();
        put(b, 0x98 + 112, 0x1020, 4);
        put(b, 0x98 + 116, 0xc0, 4);
        put(b, 0x230, 10, 4);
        put(b, 0x234, 3, 4);
        put(b, 0x238, 1, 4);
        put(b, 0x23c, 0x1060, 4);
        put(b, 0x240, 0x1070, 4);
        put(b, 0x244, 0x1080, 4);
        put(b, 0x260, 0x1000, 4);
        put(b, 0x264, 0, 4);
        put(b, 0x268, 0x10b0, 4);
        put(b, 0x270, 0x1090, 4);
        put(b, 0x280, 0, 2);
        const std::string exportName = "Exported", forward = "OTHER.Target";
        std::copy(exportName.begin(), exportName.end(), b.begin() + 0x290);
        std::copy(forward.begin(), forward.end(), b.begin() + 0x2b0);
        auto exports = bs::PEParser::parse(b);
        check(exports.exports.size() == 2, "Export holes");
        check(exports.exports[0].name == exportName && exports.exports[0].ordinal == 10, "Named export");
        check(exports.exports[1].forwarder == forward && exports.exports[1].ordinal == 12,
              "Forwarded export");
        put(b, 0x280, 3, 2);
        check(!bs::PEParser::parse(b).warnings.empty(), "Bad export ordinal");
        std::mt19937 random(0xB15C0FE);
        const Bytes text{'H', 'e', 'l', 'l', 'o', 0, 0, 0, 'W', 0, 'i', 0, 'd', 0, 'e', 0, 0, 0};
        const auto strings = bs::StringExtractor::extract(text, 4);
        bool ascii = false, wide = false;
        for (const auto& s : strings.entries) {
            ascii |= s.offset == 0 && s.length == 5 && s.encoding == bs::StringEncoding::Ascii;
            wide |= s.offset == 8 && s.length == 4 && s.encoding == bs::StringEncoding::Utf16LE;
        }
        check(ascii && wide, "ASCII and UTF16 extraction");
        const Bytes odd{0, 'W', 0, 'i', 0, 'd', 0, 'e', 0, 0, 0};
        bool oddWide = false;
        for (const auto& s : bs::StringExtractor::extract(odd, 4).entries)
            oddWide |= s.offset == 1 && s.encoding == bs::StringEncoding::Utf16LE;
        check(oddWide, "Unaligned UTF16");
        const Bytes trailing{'T', 'a', 'i', 'l'};
        check(bs::StringExtractor::extract(trailing, 4).entries.size() == 1, "ASCII at EOF");
        const Bytes unterminated{'W', 0, 'i', 0, 'd', 0, 'e', 0};
        check(bs::StringExtractor::extract(unterminated, 4).entries.empty(),
              "Wide string requires terminator");
        const Bytes nonAscii{0xe4, 0, 0xf6, 0, 0xfc, 0, 0, 0};
        bool unicode = false;
        for (const auto& s : bs::StringExtractor::extract(nonAscii, 3).entries)
            unicode |= s.offset == 0 && s.length == 3 && s.encoding == bs::StringEncoding::Utf16LE;
        check(unicode, "Non-ASCII UTF16");
        std::atomic_bool cancelled{true};
        check(bs::StringExtractor::extract(Bytes(100000, 'A'), 4, &cancelled).entries.empty(),
              "Cancelled extraction");
        check(bs::StringExtractor::extract({}, 2).entries.empty(), "Empty extraction");
        const Bytes haystack{1, 2, 1, 2, 1}, needle{1, 2, 1};
        check(bs::ByteSearch::find(haystack, needle) == 0, "First search hit");
        check(bs::ByteSearch::find(haystack, needle, 1) == 2, "Overlapping hit");
        check(!bs::ByteSearch::find(haystack, needle, 3), "Search end boundary");
        check(!bs::ByteSearch::find(haystack, {}), "Empty pattern");
        Bytes uniform(256);
        for (unsigned i = 0; i < 256; ++i)
            uniform[i] = std::uint8_t(i);
        check(std::abs(bs::EntropyAnalyzer::calculate(uniform) - 8.0) < 1e-10, "Uniform entropy");
        check(bs::EntropyAnalyzer::calculate(Bytes(100, 0)) == 0, "Constant entropy");
        check(bs::EntropyAnalyzer::calculate({}) == 0, "Empty entropy");
        const auto decoder = bs::Disassembler::create();
        const auto decoded =
            decoder->decode(Bytes{0x55, 0x57, 0x90, 0xc3}, 0x140001000, bs::InstructionSet::X64);
        check(decoded.instructions.size() == 4 && decoded.instructions[0].operands == "rbp" &&
                  decoded.instructions[3].mnemonic == "ret",
              "Basic decode");
        check(decoder->decode(Bytes{0x48}, 0, bs::InstructionSet::X64).instructions.empty(),
              "Truncated instruction");
        if (decoder->name().find("Capstone") != std::string::npos) {
            const auto mov = decoder->decode(Bytes{0x48, 0x89, 0x5c, 0x24, 0x08, 0x57}, 0x140001000,
                                             bs::InstructionSet::X64);
            check(mov.instructions.size() == 2 && mov.instructions[0].mnemonic == "mov" &&
                      mov.instructions[1].address == 0x140001005 && mov.instructions[1].operands == "rdi",
                  "Capstone instruction boundaries");
            const auto arm =
                decoder->decode(Bytes{0xc0, 0x03, 0x5f, 0xd6}, 0x1000, bs::InstructionSet::Arm64);
            check(arm.instructions.size() == 1 && arm.instructions[0].mnemonic == "ret", "ARM64 decoding");
        }
        for (int iteration = 0; iteration < 4000; ++iteration) {
            b = good;
            for (int mutation = 0; mutation < 8; ++mutation)
                b[random() % b.size()] = std::uint8_t(random());
            try {
                (void)bs::PEParser::parse(b);
            } catch (const bs::ParseError&) {
            }
        }
        if (argc > 1) {
            const bs::BinaryFile file{std::filesystem::path(argv[1])};
            const auto pe = bs::PEParser::parse(file.bytes());
            std::cout << "Real PE: " << pe.architecture() << ", " << pe.sections.size() << " sections, "
                      << pe.imports.size() << " imports, " << pe.exports.size() << " exports\n";
            for (const auto& warning : pe.warnings)
                std::cout << warning << '\n';
        }
        std::cout << "All parser tests passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
