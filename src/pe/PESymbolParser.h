#pragma once
#include "PEHeaders.h"
#include <span>
namespace bs {
void parseImports(PEImage& image, std::span<const std::uint8_t> bytes);
void parseExports(PEImage& image, std::span<const std::uint8_t> bytes);
} // namespace bs
