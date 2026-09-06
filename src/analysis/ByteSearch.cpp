#include "ByteSearch.h"
#include <algorithm>
#include <array>
namespace bs {
std::optional<std::uint64_t> ByteSearch::find(std::span<const std::uint8_t> bytes,
                                              std::span<const std::uint8_t> pattern, std::uint64_t from,
                                              const std::atomic_bool* cancel) {
    if (pattern.empty() || from > bytes.size() || pattern.size() > bytes.size() - from)
        return {};
    std::array<std::size_t, 256> skip;
    skip.fill(pattern.size());
    for (std::size_t i = 0; i + 1 < pattern.size(); ++i)
        skip[pattern[i]] = pattern.size() - 1 - i;
    for (auto pos = from; pos <= bytes.size() - pattern.size();) {
        if (cancel && cancel->load(std::memory_order_relaxed))
            return {};
        if (std::equal(pattern.begin(), pattern.end(), bytes.begin() + static_cast<std::ptrdiff_t>(pos)))
            return pos;
        pos += skip[bytes[pos + pattern.size() - 1]];
    }
    return {};
}
} // namespace bs
