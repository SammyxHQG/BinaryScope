#include "BinaryFile.h"
#include <fstream>
#include <limits>
#include <stdexcept>

namespace bs {
BinaryFile::BinaryFile(const std::filesystem::path& path) : path_(std::filesystem::absolute(path)) {
    std::ifstream stream(path_, std::ios::binary | std::ios::ate);
    if (!stream)
        throw std::runtime_error("Cannot open file. Check that it exists and you have read permission.");
    const auto length = stream.tellg();
    if (length <= 0)
        throw std::runtime_error("The file is empty or its size could not be read.");
    // Explicit resource policy, not a parser assumption. Avoid exhausting desktop memory.
    constexpr std::uint64_t maxSize = 1024ULL * 1024 * 1024;
    if (static_cast<std::uint64_t>(length) > maxSize)
        throw std::runtime_error("Files larger than 1 GiB are not supported by the snapshot reader.");
    data_.resize(static_cast<std::size_t>(length));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char*>(data_.data()), length))
        throw std::runtime_error("Could not read the entire file; it may have changed during loading.");
}
} // namespace bs
