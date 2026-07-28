add_executable(debugger main.cpp debugger.cpp cli.cpp)
target_include_directories(debugger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils)
target_compile_options(debugger PRIVATE -g -Wall -Wextra -Wpedantic)
target_link_libraries(debugger PRIVATE ${llvm_libs})