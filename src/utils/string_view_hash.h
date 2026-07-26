#pragma once

#include <string_view>

namespace debugger::utils {
    // hash by string_view
    struct StringViewHash {
        using is_transparent = void;
        size_t operator() (std::string_view sv) const {
            return std::hash<std::string_view>{}(sv);
        }
    };
}