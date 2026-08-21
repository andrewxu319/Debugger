add_executable(debugger main.cpp debugger.cpp disassembler.cpp cli.cpp utils/logging.cpp)
target_include_directories(debugger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils)
set_target_properties(debugger PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}") # put binary in build/ instead of build/src/
target_compile_options(debugger PRIVATE -g -Wall -Wextra -Wpedantic)
target_link_libraries(debugger PRIVATE ${llvm_libs})