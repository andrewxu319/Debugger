cmake_minimum_required(VERSION 3.0)

Project(Debugger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(src)
add_subdirectory(tests)