#pragma once
#include <atomic>
#include <cstdint>
#include <span>
#include <vector>
namespace bs {
enum class StringEncoding { Ascii, Utf16LE };
struct ExtractedString {
    std::uint64_t offset{};
    std::uint32_t length{};
    StringEncoding encoding{};
};
struct StringIndex {
    std::vector<ExtractedString> entries;
    bool limited{};
};
class StringExtractor {
  public:
    static StringIndex extract(std::span<const std::uint8_t> bytes, unsigned minimumLength = 4,
                               const std::atomic_bool* cancel = nullptr);
};
} // namespace bs
