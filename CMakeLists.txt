cmake_minimum_required(VERSION 3.22)
project(Mango)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
endif()

# Includes
# Use FetchContent instead of ExternalProject for fast building
include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)

# third-party
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third-party")

# termbox2
FetchContent_Declare(
    termbox2
    GIT_REPOSITORY "https://github.com/termbox/termbox2.git"
    GIT_TAG master
    SOURCE_DIR "${THIRD_PARTY_DIR}/termbox2"
)
set(TERMBOX2_LIB "${THIRD_PARTY_DIR}/termbox2/libtermbox2.a")
set(TERMBOX2_INCLUDE_DIR "${THIRD_PARTY_DIR}/termbox2")
add_custom_target(build_termbox2
    COMMAND make libtermbox2.a CFLAGS=\"-DTB_OPT_ATTR_W=64 -DTB_OPT_EGC\" --always-make
    WORKING_DIRECTORY "${THIRD_PARTY_DIR}/termbox2"
    COMMENT "Building termbox2 library"
    BYPRODUCTS "${TERMBOX2_LIB}"
)

# GSL
FetchContent_Declare(GSL
    GIT_REPOSITORY "https://github.com/microsoft/GSL"
    GIT_TAG "v4.2.0"
    GIT_SHALLOW ON
    SOURCE_DIR "${THIRD_PARTY_DIR}/GSL"
)

# json
FetchContent_Declare(json
    URL "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz"
    SOURCE_DIR "${THIRD_PARTY_DIR}/json"
)

# catch2
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.1
    SOURCE_DIR "${THIRD_PARTY_DIR}/Catch2"
)

# tree-sitter
FetchContent_Declare(
    treesitter
    GIT_REPOSITORY "https://github.com/tree-sitter/tree-sitter.git"
    GIT_TAG master
    SOURCE_DIR "${THIRD_PARTY_DIR}/tree-sitter"
)
set(TREESITTER_LIB "${THIRD_PARTY_DIR}/tree-sitter/libtree-sitter.a")
set(TREESITTER_INCLUDE_DIR "${THIRD_PARTY_DIR}/tree-sitter/lib/include")
add_custom_target(build_treesitter
    COMMAND make
    WORKING_DIRECTORY "${THIRD_PARTY_DIR}/tree-sitter"
    COMMENT "Building tree-sitter library"
    BYPRODUCTS "${TREESITTER_LIB}"
)

FetchContent_MakeAvailable(termbox2 GSL json Catch2 treesitter)

# TODO: use ICU and support grapheme cluster
# # ICU
# find_package(ICU REQUIRED COMPONENTS uc i18n data)

# Mango library
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
add_library(mango_lib STATIC)
add_dependencies(mango_lib build_termbox2 build_treesitter)
target_sources(
  mango_lib PUBLIC
  "${SRC_DIR}/buffer_manager.h"
  "${SRC_DIR}/buffer_manager.cpp"
  "${SRC_DIR}/buffer.h"
  "${SRC_DIR}/buffer.cpp"
  "${SRC_DIR}/character.h"
  "${SRC_DIR}/character.cpp"
  "${SRC_DIR}/cmd_parse.h"
  "${SRC_DIR}/cmd_parse.cpp"
  "${SRC_DIR}/cmp_menu.h"
  "${SRC_DIR}/cmp_menu.cpp"
  "${SRC_DIR}/command_manager.h"
  "${SRC_DIR}/command_manager.cpp"
  "${SRC_DIR}/cursor.h"
  "${SRC_DIR}/cursor.cpp"
  "${SRC_DIR}/editor.h"
  "${SRC_DIR}/editor.cpp"
  "${SRC_DIR}/exception.h"
  "${SRC_DIR}/exception.cpp"
  "${SRC_DIR}/filetype.h"
  "${SRC_DIR}/filetype.cpp"
  "${SRC_DIR}/frame.h"
  "${SRC_DIR}/frame.cpp"
  "${SRC_DIR}/fs.h"
  "${SRC_DIR}/fs.cpp"
  "${SRC_DIR}/syntax.h"
  "${SRC_DIR}/syntax.cpp"
  "${SRC_DIR}/keyseq_manager.h"
  "${SRC_DIR}/keyseq_manager.cpp"
  "${SRC_DIR}/logging.h"
  "${SRC_DIR}/logging.cpp"
  "${SRC_DIR}/mango_peel.h"
  "${SRC_DIR}/mango_peel.cpp"
  "${SRC_DIR}/options.h"
  "${SRC_DIR}/options.cpp"
  "${SRC_DIR}/result.h"
  "${SRC_DIR}/signal_handler.h"
  "${SRC_DIR}/signal_handler.cpp"
  "${SRC_DIR}/state.h"
  "${SRC_DIR}/state.cpp"
  "${SRC_DIR}/status_line.h"
  "${SRC_DIR}/status_line.cpp"
  "${SRC_DIR}/term.h"
  "${SRC_DIR}/term.cpp"
  "${SRC_DIR}/utils.h"
  "${SRC_DIR}/utils.cpp"
  "${SRC_DIR}/window.h"
  "${SRC_DIR}/window.cpp"
)
# TODO: better script for more languages
target_sources(
  mango_lib PUBLIC
  "${THIRD_PARTY_DIR}/tree-sitter-grammars/tree-sitter-c/src/parser.c"
  "${THIRD_PARTY_DIR}/tree-sitter-grammars/tree-sitter-cpp/src/parser.c"
  "${THIRD_PARTY_DIR}/tree-sitter-grammars/tree-sitter-cpp/src/scanner.c"
  "${THIRD_PARTY_DIR}/tree-sitter-grammars/tree-sitter-json/src/parser.c"
)
target_link_libraries(
  mango_lib
  PUBLIC ${TERMBOX2_LIB} 
  PUBLIC Microsoft.GSL::GSL 
  PUBLIC nlohmann_json::nlohmann_json
  PUBLIC ${TREESITTER_LIB}
)
target_include_directories(
  mango_lib 
  PUBLIC "${SRC_DIR}" "${TERMBOX2_INCLUDE_DIR}" "${TREESITTER_INCLUDE_DIR}"
)

target_compile_definitions(mango_lib PUBLIC TB_OPT_ATTR_W=64 TB_OPT_EGC)

# mango executable
add_executable(mgo)
target_sources(
  mgo PUBLIC
  "${SRC_DIR}/main.cpp"
)
target_link_libraries(mgo PUBLIC mango_lib)

# Tests
set(TEST_DIR "${CMAKE_SOURCE_DIR}/test")

add_executable(test)
target_sources(
  test PUBLIC
  "${TEST_DIR}/logging_test.cpp"
  "${TEST_DIR}/term_test.cpp"
  "${TEST_DIR}/xxx_manager_test.cpp"
)
target_link_libraries(test PUBLIC mango_lib Catch2::Catch2WithMain)