#include "cli.h"

#include <charconv>
#include <iostream>
#include <stdio.h>
#include <string>

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
                debugger.breakpoint(args[1]);
            } },
            { "del", [](Debugger& debugger, const std::vector<std::string_view>& args) {
                debugger.del(std::stoi(args[1].data()));
            } },
            { "info", [](Debugger& debugger, const std::vector<std::string_view>& args) {
                debugger.info(args[1]);
            } },
            { "next", [](Debugger& debugger, const std::vector<std::string_view>& args) {
                debugger.next();
            } },
            { "print", [this](Debugger& debugger, const std::vector<std::string_view>& args) {
                auto it{ this->regs_.find(args[1]) };
                if (it == this->regs_.end()) {
                    printf("Invalid print target!");
                } else {
                    debugger.print_reg(it->second);
                }
            } },
            { "set", [this](Debugger& debugger, const std::vector<std::string_view>& args) {
                auto it{ this->regs_.find(args[1]) };

                word val;
                bool success{ true };
                if (args[2].starts_with("0x")) {
                    auto [_, exception] { std::from_chars(args[2].data(), args[2].data() + args[2].size(), val, 16) };
                    if (exception != std::errc()) {
                        success = false;
                    }
                } else {
                    auto [_, exception] { std::from_chars(args[2].data(), args[2].data() + args[2].size(), val) };
                    if (exception != std::errc()) {
                        success = false;
                    }
                }

                if (success) {
                    debugger.set_reg(it->second, val);
                } else {
                    printf("Invalid input!");
                }
            } },
            { "step", [](Debugger& debugger, const std::vector<std::string_view>& args) {
                debugger.step();
            } },
            { "quit", [](Debugger& debugger, const std::vector<std::string_view>& args) {
                ;
            } }
        },
        regs_{
            { "$rip", &debugger_.regs_.rip },
            { "$rsp", &debugger_.regs_.rsp }
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