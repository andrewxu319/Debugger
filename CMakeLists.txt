cmake_minimum_required(VERSION 3.0)

Project(Debugger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(debugger src/debugger.cpp)