#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
namespace bs {
enum class InstructionSet { X86, X64, Arm64, Unsupported };
struct Instruction {
    std::uint64_t address{};
    std::vector<std::uint8_t> bytes;
    std::string mnemonic, operands;
};
struct Disassembly {
    std::vector<Instruction> instructions;
    std::string note;
};
class Disassembler {
  public:
    virtual ~Disassembler() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual Disassembly decode(std::span<const std::uint8_t> bytes, std::uint64_t address,
                                             InstructionSet architecture, std::size_t maxInstructions = 20000,
                                             const std::atomic_bool* cancel = nullptr) const = 0;
    static std::unique_ptr<Disassembler> create();
};
} // namespace bs
