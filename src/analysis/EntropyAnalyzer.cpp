#include "EntropyAnalyzer.h"
#include <array>
#include <cmath>
namespace bs {
double EntropyAnalyzer::calculate(std::span<const std::uint8_t> bytes) {
    if (bytes.empty())
        return 0;
    std::array<std::uint64_t, 256> frequency{};
    for (const auto byte : bytes)
        ++frequency[byte];
    double entropy = 0;
    for (const auto count : frequency)
        if (count) {
            const double probability = double(count) / double(bytes.size());
            entropy -= probability * std::log2(probability);
        }
    return entropy;
}
} // namespace bs
