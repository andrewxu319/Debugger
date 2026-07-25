#pragma once

#include "debugger.h"
#include "global.h"

#include <array>
#include <functional>
#include <string>
#include <string_view>

namespace debugger {
    class CLI {
    public:
        CLI(Debugger& debugger);
        void split_input();
        void prompt();

    private:
        // hash by string_view
        struct StringViewHash {
            using is_transparent = void;
            size_t operator() (std::string_view sv) const {
                return std::hash<std::string_view>{}(sv);
            }
        };

        Debugger& debugger_;
        static constexpr size_t MAX_ARGS = 4;
        std::string input_;
        std::string_view input_view_;
        std::vector<std::string_view> args_;

        using Handler = std::function<void(Debugger&, const std::vector<std::string_view>& args)>;
        std::unordered_map<std::string, Handler, StringViewHash, std::equal_to<>> commands_;
        std::unordered_map<std::string, word*, StringViewHash, std::equal_to<>> regs_;
    };
}