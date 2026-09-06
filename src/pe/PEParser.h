#pragma once
#include "PEHeaders.h"
#include <span>

namespace bs {
class PEParser {
  public:
    [[nodiscard]] static PEImage parse(std::span<const std::uint8_t> bytes);
};
} // namespace bs
