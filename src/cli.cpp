#include "cli.h"

#include <iostream>
#include <stdio.h>

namespace debugger
{
    CLI::CLI(Debugger& debugger)
        : debugger_{ debugger },
        input_{},
        input_view_{},
        args_{},
        commands_{
            { "run", [](Debugger& debugger, const std::vector<std::string_view>& args) {
                // debugger.run();
            } },
            { "continue", [](Debugger& debugger, const std::vector<std::string_view>& args) {
                debugger.cont();
            } },
            { "break", [](Debugger& debugger, const std::vector<std::string_view>& args) {
                // debugger.add_breakpoint();
            } },
            { "reg", [this](Debugger& debugger, const std::vector<std::string_view>& args) {
                auto it{ this->regs_.find(args[1]) };
                debugger.print_reg(it->second);
            } },
            { "quit", [](Debugger& debugger, const std::vector<std::string_view>& args) {
            } }
        },
        regs_{
            { "rip", &debugger_.regs_.rip },
            { "rsp", &debugger_.regs_.rsp }
            // add more
        }
    {
        args_.reserve(MAX_ARGS);
    }

    void CLI::split_input()
    {
        args_.clear();

        size_t start{};
        while (true) {
            start = input_.find_first_not_of(' ', start);
            if (start == std::string::npos) break;

            size_t end{ input_.find(' ', start) };
            if (end == std::string::npos) {
                args_.emplace_back(input_view_.substr(start));
                break;
            }
            args_.emplace_back(input_view_.substr(start, end - start));
            start = end;
        }
    }
    
    void CLI::prompt()
    {
        printf(">> ");
        std::getline(std::cin, input_);
        input_view_ = input_;
        split_input();
        if (args_.empty()) return;

        auto it{ commands_.find(args_[0]) };
        if (it == commands_.end()) {
            printf("Unknown command.\n");
            return;
        }
        it->second(debugger_, args_);
    }
}