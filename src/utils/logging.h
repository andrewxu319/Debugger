#pragma once

#include <string_view>

namespace debugger::logging {
    void error();
    void error(std::string_view msg);
}