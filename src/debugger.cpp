#include "debugger.h"

#include "LIEF/ELF.hpp"

#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>

namespace debugger
{
    Debugger::Debugger(int pid, const char* program)
        : regs_{},
        pid_{ pid },
        wait_status_{},
        debug_info_{ *[] (const char* program) {
            auto elf = LIEF::ELF::Parser::parse(std::string{ program });
            if (const LIEF::dwarf::DebugInfo* info = elf->debug_info()->as<LIEF::dwarf::DebugInfo>()) {
            return info;
            }
        }(program) },
        breakpoints_{}
    {
    }
    
    Debugger::~Debugger()
    {
        if (!WIFEXITED(wait_status_) && !WIFSIGNALED(wait_status_)) {
            kill(pid_, SIGKILL);
            waitpid(pid_, nullptr, 0);
        }
    }
    
    void Debugger::init()
    {
        waitpid(pid_, &wait_status_, 0);
        
        if (WIFSTOPPED(wait_status_)) {
            printf("Starting!\n");
        }
        
        ptrace(PTRACE_SETOPTIONS, pid_, nullptr, PTRACE_O_EXITKILL);
    }
    
    void Debugger::cont()
    {
        ptrace(PTRACE_CONT, pid_, nullptr, nullptr);
        waitpid(pid_, &wait_status_, 0);

        // program runs

        // breakpoint hit
        reg_read();
        reg_write(&regs_.rip, regs_.rip - 1);
    }
    
    void Debugger::breakpoint(word addr)
    {
        breakpoints_[addr] = ptrace(PTRACE_PEEKTEXT, pid_, addr);
        ptrace(PTRACE_POKETEXT, pid_, addr, 0xcc);
    }

    void Debugger::breakpoint(const char* fn_name)
    {
        auto fn{ debug_info_.find_function(fn_name) };
        auto fn_addr{ fn->address() };
        if (fn_addr)
        {
            breakpoint(fn_addr.value());
        }
        else
        {
            ;
        }
    }
    
    void Debugger::print_reg(const word* reg)
    {
        reg_read();
        printf("0x%016llX (%lld)\n", *reg, *reg);
    }
    
    void Debugger::reg_write(word* reg, word data)
    {
        *reg = data;
        ptrace(PTRACE_SETREGS, pid_, nullptr, &regs_);
    }
    
    void Debugger::reg_read()
    {
        ptrace(PTRACE_GETREGS, pid_, nullptr, &regs_);
    }
}

