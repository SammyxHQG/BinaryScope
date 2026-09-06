#include "Disassembler.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>
#ifdef BINARYSCOPE_CAPSTONE
#include <capstone/capstone.h>
#endif
namespace bs {
namespace {
class BasicDisassembler final : public Disassembler {
  public:
    std::string name() const override { return "Built-in minimal x86 decoder"; }
    Disassembly decode(std::span<const std::uint8_t> bytes, std::uint64_t address,
                       InstructionSet architecture, std::size_t maxInstructions,
                       const std::atomic_bool* cancel) const override {
        Disassembly result;
        if (architecture != InstructionSet::X86 && architecture != InstructionSet::X64) {
            result.note = "This backend supports only x86 and x86-64.";
            return result;
        }
        for (std::size_t i = 0; i < bytes.size() && result.instructions.size() < maxInstructions;) {
            if (cancel && cancel->load())
                return {};
            Instruction instruction;
            instruction.address = address + i;
            std::size_t length = 1;
            switch (bytes[i]) {
            case 0x90:
                instruction.mnemonic = "nop";
                break;
            case 0xc3:
                instruction.mnemonic = "ret";
                break;
            case 0xcc:
                instruction.mnemonic = "int3";
                break;
            case 0xc9:
                instruction.mnemonic = "leave";
                break;
            default:
                if (bytes[i] >= 0x50 && bytes[i] <= 0x5f) {
                    const char* r64[] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"};
                    const char* r32[] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
                    instruction.mnemonic = bytes[i] < 0x58 ? "push" : "pop";
                    instruction.operands = (architecture == InstructionSet::X64 ? r64 : r32)[bytes[i] & 7];
                } else {
                    result.note = "Stopped at an unsupported instruction. Enable Capstone for full decoding; "
                                  "remaining bytes are not labeled as instructions.";
                    return result;
                }
            }
            instruction.bytes.assign(bytes.begin() + std::ptrdiff_t(i),
                                     bytes.begin() + std::ptrdiff_t(i + length));
            result.instructions.push_back(std::move(instruction));
            i += length;
        }
        result.note = "Minimal decoder: only nop, ret, int3, leave and register push/pop. Enable Capstone "
                      "for full decoding.";
        return result;
    }
};
#ifdef BINARYSCOPE_CAPSTONE
class CapstoneDisassembler final : public Disassembler {
    struct Handle {
        csh value{};
        ~Handle() {
            if (value)
                cs_close(&value);
        }
    };
    struct InstructionDeleter {
        void operator()(cs_insn* instruction) const { cs_free(instruction, 1); }
    };

  public:
    std::string name() const override { return "Capstone 5 · Intel syntax"; }
    Disassembly decode(std::span<const std::uint8_t> bytes, std::uint64_t address,
                       InstructionSet architecture, std::size_t maxInstructions,
                       const std::atomic_bool* cancel) const override {
        Disassembly result;
        if (architecture == InstructionSet::Unsupported) {
            result.note = "Disassembly is unavailable for this architecture.";
            return result;
        }
        Handle handle;
        const auto arch = architecture == InstructionSet::Arm64 ? CS_ARCH_ARM64 : CS_ARCH_X86;
        const auto mode = architecture == InstructionSet::Arm64 ? CS_MODE_ARM
                          : architecture == InstructionSet::X64 ? CS_MODE_64
                                                                : CS_MODE_32;
        if (cs_open(arch, mode, &handle.value) != CS_ERR_OK)
            throw std::runtime_error("Could not initialize Capstone.");
        std::unique_ptr<cs_insn, InstructionDeleter> instruction(cs_malloc(handle.value));
        if (!instruction)
            throw std::bad_alloc();
        const auto* data = bytes.data();
        auto size = bytes.size();
        auto va = address;
        while (size && result.instructions.size() < maxInstructions) {
            if (cancel && cancel->load())
                return {};
            if (!cs_disasm_iter(handle.value, &data, &size, &va, instruction.get())) {
                result.note = "Decoding stopped at invalid or incomplete instruction bytes. Choose another "
                              "RVA to continue.";
                return result;
            }
            result.instructions.push_back({instruction->address,
                                           {instruction->bytes, instruction->bytes + instruction->size},
                                           instruction->mnemonic,
                                           instruction->op_str});
        }
        result.note = size ? "Instruction limit reached (20,000). Choose a later RVA to continue."
                           : "Reached the end of this section's file-backed data.";
        return result;
    }
};
#endif
} // namespace
std::unique_ptr<Disassembler> Disassembler::create() {
#ifdef BINARYSCOPE_CAPSTONE
    return std::make_unique<CapstoneDisassembler>();
#else
    return std::make_unique<BasicDisassembler>();
#endif
}
} // namespace bs
