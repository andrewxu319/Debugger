#pragma once

#include "utils/global.h"
#include "utils/string_view_hash.h"

#include <llvm/DebugInfo/DWARF/DWARFContext.h>
#include <llvm/Demangle/Demangle.h>
#include <llvm/Object/ObjectFile.h>

#include <memory>
#include <sys/user.h>
#include <unordered_map>
#include <utility>

namespace debugger {
    class Debugger {
    public:
        Debugger(int pid, const char* program);
        ~Debugger();

        void init();
        void cont();
        void breakpoint(word vaddr);
        void breakpoint(std::string_view fn_name);
        void print_reg(const word* reg);
        void reg_write(word* reg, word data);
        
        struct user_regs_struct regs_;

    private:
        // function symbol
        struct FnSym {
            word vaddr;
            uint64_t size;
        };

        void get_base_addr();
        void build_msymtabs();
        void reg_read();

        int pid_;
        int wait_status_;
        word base_addr_;

        llvm::object::OwningBinary<llvm::object::ObjectFile> obj_file_;
        llvm::object::ObjectFile* obj_;
        std::unique_ptr<llvm::DWARFContext> dwarf_ctx_;
        llvm::ItaniumPartialDemangler demangler_;

        llvm::SmallString<256> mangled_buffer_;
        static constexpr size_t demangled_buffer_capacity_ = 128;
        char demangled_buffer_[demangled_buffer_capacity_]; // does this need to be malloc'ed?
        
        std::unordered_map<std::string, FnSym, utils::StringViewHash, std::equal_to<>> msymtabs_;
        std::unordered_map<word, word> breakpoints_;
    };
}