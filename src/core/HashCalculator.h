#pragma once
#include <QString>
#include <cstdint>
#include <span>
namespace bs {
struct Hashes {
    QString sha256, md5;
};
class HashCalculator {
  public:
    static Hashes calculate(std::span<const std::uint8_t> bytes);
};
} // namespace bs
