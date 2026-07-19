add_executable(debugger main.cpp debugger.cpp cli.cpp)
target_compile_options(debugger PRIVATE -g)