#include "debugger.h"
#include "cli.h"

#include <cstdio>
#include <cassert>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <vector>

using namespace debugger;

// void handle_sig_int(int signal) {
//     if (signal == 2) {
//         kill(pid, SIGKILL);
//         waitpid(pid, nullptr, 0);
//     }
// }

int main(int argc, char* argv[]) {
    // validate arguments
    assert(argc >= 2);

    char* program{ argv[1] };

    pid_t pid{ fork() };

    // target/debuggee
    if (pid == 0) {
        std::vector<char*> child_args(argc);
        for (int i{}; i < argc - 1; i++) {
            child_args[i] = argv[i + 1];
        }
        child_args.back() = nullptr;
        if (ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) != 0) {
            printf("ptrace error\n");
            return 1;
        }
        execvp(program, child_args.data());
        perror("Target program launch failed!\n");
        return 1;
    }
    
    // debugger
    else {
        Debugger debugger{ pid, program };
        CLI cli{ debugger };

        // handle ctrl-c
        // struct sigaction sig_int_handler{};
        // sig_int_handler.sa_handler = handle_sig_int;
        // sigemptyset(&sig_int_handler.sa_mask);
        // sig_int_handler.sa_flags = 0;
        // sigaction(SIGINT, &sig_int_handler, nullptr);

        debugger.init();
        cli.test();
        while (true) {
            cli.prompt();
        }
    }
}