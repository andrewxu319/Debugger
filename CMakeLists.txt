cmake_minimum_required(VERSION 3.21)

Project(Debugger LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)  

# linker
find_program(MOLD_PATH mold)
find_program(LLD_PATH ld.lld)
if(MOLD_PATH)
    add_link_options("-fuse-ld=mold")
elseif(LLD_PATH)
    add_link_options("-fuse-ld=lld")
endif()

# set(LIEF_USE_CCACHE OFF CACHE BOOL "Use ccache to speed up compilation" FORCE) 
find_program(SCCACHE_PROGRAM sccache)
if(SCCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
    message(STATUS "Using sccache.")
else()
    message(WARNING "sccache not found. Building without it.")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(BUILD_SHARED_LIBS ON CACHE INTERNAL "")
endif()


##############################################################################

# Use LIEF as an embedded/vendored project
# ========================================

# LIEF build config. Set the default options for LIEF's project setup
set(LIEF_TESTS OFF CACHE BOOL                      "Enable tests"                               FORCE)
set(LIEF_PYTHON_API OFF CACHE BOOL                 "Enable Python Bindings"                     FORCE)
set(LIEF_C_API OFF CACHE BOOL                      "C API"                                      FORCE)
set(LIEF_EXAMPLES OFF CACHE BOOL                   "Build LIEF C++ examples"                    FORCE)
set(LIEF_FORCE32 OFF CACHE BOOL                    "Force build LIEF 32 bits version"           FORCE)
set(LIEF_USE_CCACHE ON CACHE BOOL                 "Use ccache to speed up compilation"         FORCE)
set(LIEF_EXTRA_WARNINGS OFF CACHE BOOL             "Enable extra warning from the compiler"     FORCE)
set(LIEF_LOGGING OFF CACHE BOOL                    "Enable logging"                             FORCE)
set(LIEF_LOGGING_DEBUG OFF CACHE BOOL              "Enable debug logging"                       FORCE)
set(LIEF_ENABLE_JSON OFF CACHE BOOL                "Enable JSON-related APIs"                   FORCE)
set(LIEF_OPT_NLOHMANN_JSON_EXTERNAL OFF CACHE BOOL "Use nlohmann/json externally"               FORCE)
set(LIEF_FORCE_API_EXPORTS OFF CACHE BOOL          "Force exports of API symbols"               FORCE)
set(LIEF_PY_LIEF_EXT OFF CACHE BOOL                "Use a pre-installed version of LIEF for the bindings" FORCE)
set(LIEF_PRECOMPILED OFF CACHE BOOL                "Use a pre-compiled version of LIEF" FORCE)
set(LIEF_RUST_API OFF CACHE BOOL                   "Generate the C++ bridge for Rust's cxx" FORCE)
set(LIEF_DISABLE_EXCEPTIONS ON CACHE BOOL         "Disable C++ exceptions on the core library" FORCE)
set(LIEF_SO_VERSION OFF CACHE BOOL                 "Embed versioning for LIEF shared library target" FORCE)
set(LIEF_COMPILE_DOC_EXAMPLES OFF CACHE BOOL       "Compile C++ examples in doc/code/" FORCE)

set(LIEF_DOC OFF CACHE BOOL "Build LIEF docs" FORCE)
set(LIEF_PYTHON_API OFF CACHE BOOL "Build LIEF Python API" FORCE)
set(LIEF_EXAMPLES OFF CACHE BOOL "Build LIEF examples" FORCE)
set(LIEF_TESTS OFF CACHE BOOL "Build LIEF tests" FORCE)
set(LIEF_DISABLE_FROZEN ON CACHE BOOL "Disable Frozen even if it is supported"     FORCE)
set(LIEF_RUNTIME OFF CACHE BOOL        "Enable runtime features" FORCE)

set(LIEF_ELF ON CACHE BOOL            "Build LIEF with ELF module"                 FORCE)
set(LIEF_PE OFF CACHE BOOL             "Build LIEF with PE module"                  FORCE)
set(LIEF_COFF OFF CACHE BOOL           "Build LIEF with COFF module"                FORCE)
set(LIEF_MACHO OFF CACHE BOOL          "Build LIEF with MachO module"               FORCE)

set(LIEF_DEX OFF CACHE BOOL            "Build LIEF with DEX module"                 FORCE)
set(LIEF_ART OFF CACHE BOOL            "Build LIEF with ART module"                 FORCE)

# Extended features
set(LIEF_DEBUG_INFO ON CACHE BOOL        "Build LIEF with DWARF/PDB support"              FORCE)
set(LIEF_OBJC OFF CACHE BOOL              "Build LIEF with ObjC metadata support"          FORCE)
set(LIEF_DYLD_SHARED_CACHE OFF CACHE BOOL "Build LIEF with Dyld shared cache support"      FORCE)
set(LIEF_ASM OFF CACHE BOOL               "Build LIEF with assembler/disassembler support" FORCE)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING "CRT set")
endif()


# If you have LIEF as a submodule in a directory, then you can add it to this
# project with ``add_subdirectory``
# NOTE: This submodule does not exist for this example, but it does the same
# thing as FetchContent without the download part
set(vendorLIEF_submodule_dir "${CMAKE_CURRENT_LIST_DIR}/libs/LIEF")
if(EXISTS "${vendorLIEF_submodule_dir}")
  add_subdirectory("${vendorLIEF_submodule_dir}")

# Else, we'll specify how to obtain LIEF another way (downloading)
else()
  # CMake 3.11 has FetchContent which provides a simple experience for what
  # we're trying to do
  cmake_minimum_required(VERSION 3.11)

  # URL of the LIEF repo (Can be your fork)
  set(LIEF_GIT_URL "https://github.com/lief-project/LIEF.git")

  # LIEF's version to be used (can be 'main')
  set(LIEF_VERSION 0.13.0)

  include(FetchContent)
  FetchContent_Declare(LIEF
    GIT_REPOSITORY  "${LIEF_GIT_URL}"
    GIT_TAG         ${LIEF_VERSION}
    # You may specify an existing LIEF source directory if you don't want to
    # download. Just comment out the above ``GIT_*`` commands and uncoment the
    # following ``SOURCE_DIR`` line
    #SOURCE_DIR      "${CMAKE_CURRENT_LIST_DIR}/../../.."
    )

  if(${CMAKE_VERSION} VERSION_LESS "3.14.0")
    # CMake 3.11 to 3.13 needs more verbose method to make LIEF available
    FetchContent_GetProperties(LIEF)
    if(NOT LIEF_POPULATED)
      FetchContent_Populate(LIEF)
      add_subdirectory(${LIEF_SOURCE_DIR} ${LIEF_BINARY_DIR})
    endif()
  else()
    # CMake 3.14+ has single function to make LIEF available (recommended)
    FetchContent_MakeAvailable(LIEF)
  endif()
endif()


add_subdirectory(src)
add_subdirectory(tests)