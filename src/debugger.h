#pragma once

#include <sys/user.h>

namespace debugger {
    class Debugger {
    public:
        Debugger(int pid);
        ~Debugger();

        void init();
        void cont();
        void reg_read();
        
        struct user_regs_struct regs_;

    private:
        int pid_;
        int wait_status_;
    };
}