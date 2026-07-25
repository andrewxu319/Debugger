#pragma once

#include "global.h"

#include "LIEF/DWARF.hpp"

#include <sys/user.h>
#include <unordered_map>

namespace debugger {
    class Debugger {
    public:
        Debugger(int pid, const char* program);
        ~Debugger();

        void init();
        void cont();
        void breakpoint(word addr);
        void breakpoint(const char* fn_name);
        void print_reg(const word* reg);
        void reg_write(word* reg, word data);
        
        struct user_regs_struct regs_;

    private:
        void reg_read();

        int pid_;
        int wait_status_;
        const LIEF::dwarf::DebugInfo& debug_info_;
        std::unordered_map<word, word> breakpoints_;
    };
}