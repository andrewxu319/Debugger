#include "cli.h"

#include <charconv>
#include <iostream>
#include <stdio.h>
#include <string>

namespace debugger
{
    CLI::CLI([[maybe_unused]] Debugger& debugger)
        : debugger_{ debugger },
        input_{},
        input_view_{},
        args_{},
        commands_{
            { "backtrace", []([[maybe_unused]] Debugger& debugger, [[maybe_unused]] const std::vector<std::string_view>& args) {
                debugger.backtrace();
            } },
            { "break", []([[maybe_unused]] Debugger& debugger, [[maybe_unused]] const std::vector<std::string_view>& args) {
                debugger.breakpoint(args[1]);
            } },
            { "continue", []([[maybe_unused]] Debugger& debugger, [[maybe_unused]] const std::vector<std::string_view>& args) {
                debugger.cont();
            } },
            { "del", []([[maybe_unused]] Debugger& debugger, [[maybe_unused]] const std::vector<std::string_view>& args) {
                debugger.del(std::stoi(args[1].data()));
            } },
            { "info", []([[maybe_unused]] Debugger& debugger, [[maybe_unused]] const std::vector<std::string_view>& args) {
                debugger.info(args[1]);
            } },
            { "next", []([[maybe_unused]] Debugger& debugger, [[maybe_unused]] const std::vector<std::string_view>& args) {
                debugger.next();
            } },
            { "print", [this]([[maybe_unused]] Debugger& debugger, [[maybe_unused]] const std::vector<std::string_view>& args) {
                auto it{ this->regs_.find(args[1]) };
                if (it == this->regs_.end()) {
                    printf("Invalid print target!");
                } else {
                    debugger.print_reg(it->second);
                }
            } },
            { "run", []([[maybe_unused]] Debugger& debugger, [[maybe_unused]] const std::vector<std::string_view>& args) {
                // debugger.run();
            } },
            { "set", [this]([[maybe_unused]] Debugger& debugger, [[maybe_unused]] const std::vector<std::string_view>& args) {
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
            { "step", []([[maybe_unused]] Debugger& debugger, [[maybe_unused]] const std::vector<std::string_view>& args) {
                debugger.step();
            } },
            { "quit", []([[maybe_unused]] Debugger& debugger, [[maybe_unused]] const std::vector<std::string_view>& args) {
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
            start = input_view_.find_first_not_of(' ', start);
            if (start == std::string::npos) break;

            size_t end{ input_view_.find(' ', start) };
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
    
    void CLI::test()
    {
        const std::vector<std::string> commands{
            "break test_program.cpp:11",
            "continue",
            "backtrace"
        };
        for (const std::string& command : commands) {
            input_view_ = command;
            split_input();
            if (args_.empty()) continue;

            auto it{ commands_.find(args_[0]) };
            if (it == commands_.end()) {
                printf("Unknown command.\n");
                return;
            }
            it->second(debugger_, args_);
        }
    }
}