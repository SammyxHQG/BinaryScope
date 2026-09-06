#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace bs {
// An owned snapshot: later changes to the original file cannot invalidate views.
class BinaryFile {
  public:
    explicit BinaryFile(const std::filesystem::path& path);
    [[nodiscard]] std::span<const std::uint8_t> bytes() const { return data_; }
    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
    std::vector<std::uint8_t> data_;
};
} // namespace bs
