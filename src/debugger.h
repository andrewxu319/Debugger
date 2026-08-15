#pragma once

#include "disassembler.h"
#include "utils/global.h"
#include "utils/string_view_hash.h"

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/DebugInfo/DWARF/LowLevel/DWARFUnwindTable.h>
#include <llvm/DebugInfo/Symbolize/Symbolize.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Object/ObjectFile.h>

#include <memory>
#include <sys/user.h>
#include <unordered_map>
#include <utility>

namespace debugger {
    class Debugger {
        // function symbol
        struct FnSym {
            word vaddr;
            uint64_t size;
        };

    public:
        Debugger(int pid, const char* program);
        ~Debugger();

        void init();
        void cont();
        void breakpoint(word vaddr);
        void breakpoint(std::string_view arg);
        void breakpoint(std::string_view file_name, size_t line);
        void del(size_t idx);
        void backtrace();
        void info(std::string_view cmd);
        void print_reg(const word* reg);
        void set_reg(word* reg, word data);
        void step();
        void next();
        
        struct user_regs_struct regs_{};

    private:
        void get_base_addr();
        void build_msymtabs();
        void reg_read();
        void set_byte(word vaddr, byte val);
        bool step_through_breakpoint(word v_rip);
        llvm::DWARFDebugLine::Row get_src_row_info(word vaddr);
        word evaluate_cfa(const llvm::dwarf::UnwindLocation& rule);
        word evaluate_ra(const llvm::dwarf::UnwindLocation& rule, word cfa, word ra_reg);

        Disassembler disassembler_{};

        int pid_{};
        int wait_status_{};
        word base_addr_{};
        bool regs_updated_{};

        llvm::object::OwningBinary<llvm::object::ObjectFile> obj_file_{};
        const llvm::object::ObjectFile* obj_{};
        llvm::Triple triple_{};
        std::unique_ptr<llvm::DWARFContext> dwarf_ctx_{};
        llvm::ItaniumPartialDemangler demangler_{};
        llvm::symbolize::LLVMSymbolizer symbolizer_{};
        llvm::DWARFCompileUnit* current_cu_{}; // TODO: update the current cu if needed
        const llvm::DWARFDebugFrame* eh_frame_{};

        llvm::SmallString<256> mangled_buffer_{};
        static constexpr size_t demangled_buffer_capacity_ = 128;
        char demangled_buffer_[demangled_buffer_capacity_]{}; // does this need to be malloc'ed?
        
        struct Breakpoint {
            llvm::DILineInfo info;
            word vaddr;
            byte data; // not necessarily the instruction because instructions are variable length
        };
        std::unordered_map<std::string, FnSym, utils::StringViewHash, std::equal_to<>> msymtabs_{};
        std::unordered_map<word, Breakpoint*> breakpoints_lookup_{}; // index by vaddr
        std::vector<std::unique_ptr<Breakpoint>> breakpoints_{};
        bool breakpoints_sorted_{};
    };
}