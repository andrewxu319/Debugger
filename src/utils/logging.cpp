#include "logging.h"

#include <cstdio>

namespace debugger::logging
{
    void error()
    {
        printf("Error!\n");
    }
    
    void error(std::string_view msg)
    {
        printf("Error! %s\n", msg.data());
    }
}