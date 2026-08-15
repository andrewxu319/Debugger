#pragma once

#include "global.h"

#include <sys/user.h>
#include <unordered_map>

namespace debugger::utils {
    static const std::unordered_map<word, word user_regs_struct::*> reg_numbers_x86_64{
        { 0,  &user_regs_struct::rax },
        { 1,  &user_regs_struct::rdx },
        { 2,  &user_regs_struct::rcx },
        { 3,  &user_regs_struct::rbx },
        { 4,  &user_regs_struct::rsi },
        { 5,  &user_regs_struct::rdi },
        { 6,  &user_regs_struct::rbp },
        { 7,  &user_regs_struct::rsp },
        { 8,  &user_regs_struct::r8 },
        { 9,  &user_regs_struct::r9 },
        { 10, &user_regs_struct::r10 },
        { 11, &user_regs_struct::r11 },
        { 12, &user_regs_struct::r12 },
        { 13, &user_regs_struct::r13 },
        { 14, &user_regs_struct::r14 },
        { 15, &user_regs_struct::r15 },
        { 16, &user_regs_struct::rip },
        { 49, &user_regs_struct::eflags },
        { 50, &user_regs_struct::es },
        { 51, &user_regs_struct::cs },
        { 52, &user_regs_struct::ss },
        { 53, &user_regs_struct::ds },
        { 54, &user_regs_struct::fs },
        { 55, &user_regs_struct::gs },
        { 58, &user_regs_struct::fs_base },
        { 59, &user_regs_struct::gs_base }
    };
}