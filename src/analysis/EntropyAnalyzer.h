#pragma once
#include <cstdint>
#include <span>
namespace bs {
class EntropyAnalyzer {
  public:
    [[nodiscard]] static double calculate(std::span<const std::uint8_t> bytes);
};
} // namespace bs
