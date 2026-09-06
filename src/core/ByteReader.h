#pragma once
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace bs {
class ParseError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};
class ByteReader {
  public:
    explicit ByteReader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
    void require(std::uint64_t offset, std::uint64_t count) const {
        if (offset > bytes_.size() || count > bytes_.size() - offset)
            throw ParseError("Truncated or corrupted PE: field extends beyond the file.");
    }
    template <class T> [[nodiscard]] T read(std::uint64_t offset) const {
        static_assert(std::is_unsigned_v<T>);
        require(offset, sizeof(T));
        std::uint64_t value = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i)
            value |= std::uint64_t(bytes_[static_cast<std::size_t>(offset) + i]) << (i * 8);
        return static_cast<T>(value);
    }
    [[nodiscard]] std::string fixedString(std::uint64_t offset, std::size_t length) const {
        require(offset, length);
        std::string result;
        for (std::size_t i = 0; i < length && bytes_[offset + i]; ++i)
            result.push_back(static_cast<char>(bytes_[offset + i]));
        return result;
    }

  private:
    std::span<const std::uint8_t> bytes_;
};
} // namespace bs
