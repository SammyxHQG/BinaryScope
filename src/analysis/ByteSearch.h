#pragma once
#include <atomic>
#include <cstdint>
#include <optional>
#include <span>
namespace bs {
class ByteSearch {
  public:
    static std::optional<std::uint64_t> find(std::span<const std::uint8_t> bytes,
                                             std::span<const std::uint8_t> pattern, std::uint64_t from = 0,
                                             const std::atomic_bool* cancel = nullptr);
};
} // namespace bs
