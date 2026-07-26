cmake_minimum_required(VERSION 3.0)

Project(Debugger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# libelfin
include(ExternalProject)

set(LIBELFIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/libelfin")

ExternalProject_Add(libelfin_build
    SOURCE_DIR        "${LIBELFIN_DIR}"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     make
    BUILD_IN_SOURCE   1
    INSTALL_COMMAND   ""
)

add_library(dwarf++ STATIC IMPORTED)
set_target_properties(dwarf++ PROPERTIES
    IMPORTED_LOCATION "${LIBELFIN_DIR}/dwarf/libdwarf++.a"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBELFIN_DIR}/dwarf;${LIBELFIN_DIR}/elf"
)
add_dependencies(dwarf++ libelfin_build)

add_library(elf++ STATIC IMPORTED)
set_target_properties(elf++ PROPERTIES
    IMPORTED_LOCATION "${LIBELFIN_DIR}/elf/libelf++.a"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBELFIN_DIR}/dwarf;${LIBELFIN_DIR}/elf"
)
add_dependencies(elf++ libelfin_build)
##

add_subdirectory(src)
add_subdirectory(tests)
