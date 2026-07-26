#pragma once

#include "utils/global.h"
#include "utils/string_view_hash.h"

#include "elf++.hh"
#include "dwarf++.hh"

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
        struct ElfSymAddrSize {
            word vaddr;
            uint64_t size;
        };
        
        struct ElfSymNameSize {
            std::string name;
            uint64_t size;
        };

        std::unique_ptr<char, void(*)(void*)> demangle(const char* fn_name);
        void build_symbol_map();
        void reg_read();

        int pid_;
        int wait_status_;
        word base_addr_;
        elf::elf elf_;
        dwarf::dwarf dwarf_;
        std::unordered_map<std::string, ElfSymAddrSize, utils::StringViewHash, std::equal_to<>> vaddr_by_fn_;
        std::unordered_map<word, ElfSymNameSize> fn_by_vaddr_;
        std::unordered_map<word, word> breakpoints_;
    };
}