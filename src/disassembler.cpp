#include "disassembler.h"
#include "utils/logging.h"

#include <llvm/ADT/StringExtras.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/TargetParser/Triple.h>

using namespace llvm;

namespace debugger
{
    Disassembler::Disassembler()
    {
    }
    
    void Disassembler::setup(const llvm::object::ObjectFile* obj)
    {
        obj_ = obj;

        InitializeNativeTarget();
        InitializeNativeTargetDisassembler();
        InitializeNativeTargetAsmPrinter();
        
        // https://gist.github.com/larkmjc/d94b72fa3d580ea2037e0a4dc5e2fc5b
        std::string triple{ obj_->makeTriple().getTriple() };
        std::string error{};
        const Target* target{ TargetRegistry::lookupTarget(triple, error) };
        llvm::MCTargetOptions options{};

        try {
            reg_info_.reset(target->createMCRegInfo(triple));
            asm_info_.reset(target->createMCAsmInfo(*reg_info_, triple, options));
            inst_info_.reset(target->createMCInstrInfo());
            inst_analysis_.reset(target->createMCInstrAnalysis(inst_info_.get()));
            subtarget_info_.reset(target->createMCSubtargetInfo(triple, "", ""));
            ctx_ = std::make_unique<MCContext>(Triple{ triple }, asm_info_.get(), reg_info_.get(), subtarget_info_.get());
            disassembler_.reset(target->createMCDisassembler(*subtarget_info_, *ctx_));
        } catch (const std::exception& exception) {
            logging::error(error);
        }
        

        // find .text section
        bool found{ false };
        for (const object::SectionRef& section : obj_->sections()) {
            auto name{ section.getName() };
            if (!name) {
                consumeError(name.takeError());
                break;
            }
            if (*name == ".text") {
                text_sec_vaddr_ = section.getAddress();
                text_sec_size_ = section.getSize();
                text_sec_ = &section;
                found = true;
                break;
            }
        }

        if (!found) {
            logging::error();
        }

        auto contents{ text_sec_->getContents() };
        if (!contents) {
            consumeError(contents.takeError());
            logging::error();
        }
        contents_ = contents.get();
    }
    
    bool Disassembler::is_call(const Inst& inst)
    {
        return inst_analysis_->isCall(inst.inst);
    }
    
    Disassembler::Inst Disassembler::analyze_inst(word vaddr)
    {
        assert(vaddr >= text_sec_vaddr_ && vaddr < text_sec_vaddr_ + text_sec_size_);

        word offset{ vaddr - text_sec_vaddr_ };
        ArrayRef<byte> bytes{ arrayRefFromStringRef(contents_.substr(offset, contents_.size() - offset)) };

        MCInst inst{};
        size_t inst_size{};
        MCDisassembler::DecodeStatus status{ disassembler_->getInstruction(
            inst, inst_size, bytes, vaddr, nulls()
        ) };
        assert(status == MCDisassembler::Success);
        return Inst{ inst, inst_size };
    }
}