#pragma once

#include "utils/global.h"

#include <llvm/MC/MCAsmInfo.h>
#include <llvm/MC/MCContext.h>
#include <llvm/MC/MCDisassembler/MCDisassembler.h>
#include <llvm/MC/MCInstrAnalysis.h>
#include <llvm/MC/MCInstrInfo.h>
#include <llvm/MC/MCRegisterInfo.h>
#include <llvm/MC/MCSubtargetInfo.h>
#include <llvm/Object/ObjectFile.h>

namespace debugger {
    class Disassembler {
    public:
        struct Inst {
            llvm::MCInst inst;
            size_t size;
        };

        Disassembler();
        
        void setup(const llvm::object::ObjectFile* obj);
        Inst analyze_inst(word vaddr);
        bool is_call(const Inst& inst);

    private:
        const llvm::object::ObjectFile* obj_{};
        std::unique_ptr<const llvm::MCRegisterInfo> reg_info_{};
        std::unique_ptr<const llvm::MCAsmInfo> asm_info_{};
        std::unique_ptr<const llvm::MCInstrInfo> inst_info_{};
        std::unique_ptr<const llvm::MCInstrAnalysis> inst_analysis_{};
        std::unique_ptr<const llvm::MCSubtargetInfo> subtarget_info_{};
        std::unique_ptr<llvm::MCContext> ctx_{};
        std::unique_ptr<const llvm::MCDisassembler> disassembler_{};
        llvm::StringRef contents_{};

        word text_sec_vaddr_{};
        uint64_t text_sec_size_{};
        const llvm::object::SectionRef* text_sec_{};
    };
}