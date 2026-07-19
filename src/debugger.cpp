#include "debugger.h"

#include <cstdio>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>

namespace debugger
{
    Debugger::Debugger(int pid)
        : pid_{ pid },
        wait_status_{},
        regs_{}
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

        // if hit breakpoint, return
    }
    
    void Debugger::reg_read()
    {
        ptrace(PTRACE_GETREGS, pid_, nullptr, &regs_);
    }
}

