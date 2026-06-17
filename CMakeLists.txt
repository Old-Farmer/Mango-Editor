cmake_minimum_required(VERSION 3.22)
project(Charxed)

set(CMAKE_CXX_STANDARD 17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
endif()

# Includes
# Use FetchContent instead of ExternalProject for fast building
include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# third-party
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third-party")

# third party versions
set(TERMBOX2_VERSION "dev")
set(UTF8PROC_VERSION "v2.11.3")
set(GSL_VERSION "v4.2.0")
set(JSON_VERSION "v3.12.0")
set(CATCH2_VERSION "v3.15.1")
set(TREESITTER_VERSION "v0.26.9")
set(FMT_VERSION "12.1.0")
set(TS_GRAMMARS
    c    "v0.24.2"
    cpp  "v0.23.4"
    json "v0.24.8"
)

# Extract TS_GRAMMARS_LANGS
set(TS_GRAMMAR_LANGS)
list(LENGTH TS_GRAMMARS N)
math(EXPR LAST "${N} - 1")
foreach(I RANGE 0 ${LAST} 2)
    list(GET TS_GRAMMARS ${I} LANG)
    list(APPEND TS_GRAMMAR_LANGS ${LANG})
endforeach()

# termbox2
FetchContent_Declare(
    termbox2
    GIT_REPOSITORY "https://github.com/Old-Farmer/termbox2.git"
    GIT_TAG ${TERMBOX2_VERSION}
    GIT_SHALLOW ON
    GIT_PROGRESS ON
    # Don't specify SOURCE_DIR because we use make in the source dir
)
FetchContent_Populate(termbox2)
set(TERMBOX2_LIB "${termbox2_SOURCE_DIR}/libtermbox2.a")
set(TERMBOX2_INCLUDE_DIR "${termbox2_SOURCE_DIR}")
add_custom_target(build_termbox2
    COMMAND
    ${CMAKE_COMMAND} -E env
    CC=${CMAKE_C_COMPILER}
    CXX=${CMAKE_CXX_COMPILER}
    make libtermbox2.a
    CFLAGS=\"-DTB_OPT_ATTR_W=64 -DTB_OPT_EGC -O2\" --always-make
    WORKING_DIRECTORY "${termbox2_SOURCE_DIR}"
    COMMENT "Building termbox2 library"
    BYPRODUCTS "${TERMBOX2_LIB}"
)

# utf8proc
FetchContent_Declare(
    utf8proc
    GIT_REPOSITORY "https://github.com/JuliaStrings/utf8proc.git"
    GIT_TAG ${UTF8PROC_VERSION}
    GIT_SHALLOW ON
    GIT_PROGRESS ON
    SOURCE_DIR "${THIRD_PARTY_DIR}/utf8proc"
)
FetchContent_MakeAvailable(utf8proc)

# GSL
FetchContent_Declare(
    GSL
    GIT_REPOSITORY "https://github.com/microsoft/GSL"
    GIT_TAG ${GSL_VERSION}
    GIT_SHALLOW ON
    GIT_PROGRESS ON
    SOURCE_DIR "${THIRD_PARTY_DIR}/GSL"
)
FetchContent_MakeAvailable(GSL)

# json
FetchContent_Declare(json
    URL "https://github.com/nlohmann/json/releases/download/${JSON_VERSION}/json.tar.xz"
    SOURCE_DIR "${THIRD_PARTY_DIR}/json"
)
FetchContent_MakeAvailable(json)

# catch2
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        ${CATCH2_VERSION}
    GIT_SHALLOW ON
    GIT_PROGRESS ON
    SOURCE_DIR "${THIRD_PARTY_DIR}/Catch2"
)
FetchContent_MakeAvailable(Catch2)

# tree-sitter
FetchContent_Declare(
    treesitter
    GIT_REPOSITORY "https://github.com/tree-sitter/tree-sitter.git"
    GIT_TAG ${TREESITTER_VERSION}
    GIT_SHALLOW ON
    GIT_PROGRESS ON
    SOURCE_DIR "${THIRD_PARTY_DIR}/tree-sitter"
)
FetchContent_MakeAvailable(treesitter)

# FetchContent_Declare(
#     json_rpc_cxx
#     GIT_REPOSITORY "https://github.com/jsonrpcx/json-rpc-cxx.git"
#     GIT_TAG master
#     GIT_SHALLOW ON
#     GIT_PROGRESS ON
#     SOURCE_DIR "${THIRD_PARTY_DIR}/json-rpc-cxx"
# )
# set(JSON_RPC_CXX_INCLUDE_DIR "${THIRD_PARTY_DIR}/json-rpc-cxx/include")
# FetchContent_MakeAvailable(json_rpc_cxx)

# TODO: maybe we can use nvim-treesitter query?
# FetchContent_Declare(
#     nvim_treesitter
#     GIT_REPOSITORY "https://github.com/nvim-treesitter/nvim-treesitter.git"
#     GIT_TAG master
#     SOURCE_DIR "${THIRD_PARTY_DIR}/nvim-treesitter"
# )

# tree-sitter-grammars
set(TS_GRAMMAR_DIR "${THIRD_PARTY_DIR}/tree-sitter-grammars")
list(LENGTH TS_GRAMMARS N)
math(EXPR N "${N} - 1")
foreach(I RANGE 0 ${N} 2)
    list(GET TS_GRAMMARS ${I} LANG)

    math(EXPR J "${I} + 1")
    list(GET TS_GRAMMARS ${J} TAG)

    set(NAME ts_grammar_${LANG})

    FetchContent_Declare(
        ${NAME}
        GIT_REPOSITORY "https://github.com/tree-sitter/tree-sitter-${LANG}.git"
        GIT_TAG ${TAG}
        GIT_SHALLOW ON
        GIT_PROGRESS ON
        SOURCE_DIR "${TS_GRAMMAR_DIR}/tree-sitter-${LANG}"
    )
    # Use populate Instead of MakeAvailabe to avoid target conflicts.
    FetchContent_Populate(${NAME})
endforeach()

# fmt
# ref https://fmt.dev/12.0/get-started/
# TODO: We link fmt as static lib,
# but when packaging the fmt lib will still be packaged.
# Try remove it from package.
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG ${FMT_VERSION}
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_DIR "${THIRD_PARTY_DIR}/fmt"
)
FetchContent_MakeAvailable(fmt)

# TODO: maybe use ICU to support more unicode features
# # ICU
# find_package(ICU REQUIRED COMPONENTS uc i18n data)

set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")

# commit hash
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
# tag
execute_process(
    COMMAND git describe --tags --abbrev=0
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
configure_file("${SRC_DIR}/version.h.in" "version.h")

# Charxed library
set(SRC_LSP_DIR "${CMAKE_SOURCE_DIR}/src/lsp")
add_library(charxed_lib STATIC)
add_dependencies(charxed_lib build_termbox2)
target_sources(
  charxed_lib PUBLIC
  "${SRC_LSP_DIR}/client.h"
  "${SRC_LSP_DIR}/client.cpp"
  "${SRC_LSP_DIR}/rpc.h"
  "${SRC_LSP_DIR}/rpc.cpp"
  "${SRC_DIR}/buffer_manager.h"
  "${SRC_DIR}/buffer_manager.cpp"
  "${SRC_DIR}/buffer_view.h"
  "${SRC_DIR}/buffer_view.cpp"
  "${SRC_DIR}/buffer.h"
  "${SRC_DIR}/buffer.cpp"
  "${SRC_DIR}/character.h"
  "${SRC_DIR}/character.cpp"
  "${SRC_DIR}/clipboard.h"
  "${SRC_DIR}/clipboard.cpp"
  "${SRC_DIR}/cmd_parse.h"
  "${SRC_DIR}/cmd_parse.cpp"
  "${SRC_DIR}/cmp_menu.h"
  "${SRC_DIR}/cmp_menu.cpp"
  "${SRC_DIR}/command_manager.h"
  "${SRC_DIR}/command_manager.cpp"
  "${SRC_DIR}/commands.cpp"
  "${SRC_DIR}/completer.h"
  "${SRC_DIR}/completer.cpp"
  "${SRC_DIR}/cursor.h"
  "${SRC_DIR}/cursor.cpp"
  "${SRC_DIR}/diff.h"
  "${SRC_DIR}/diff.cpp"
  "${SRC_DIR}/draw.h"
  "${SRC_DIR}/draw.cpp"
  "${SRC_DIR}/editor.h"
  "${SRC_DIR}/editor.cpp"
  "${SRC_DIR}/event_loop.h"
  "${SRC_DIR}/event_loop.cpp"
  "${SRC_DIR}/editor_event_manager.h"
  "${SRC_DIR}/editor_event_manager.cpp"
  "${SRC_DIR}/exception.h"
  "${SRC_DIR}/exception.cpp"
  "${SRC_DIR}/explorer.h"
  "${SRC_DIR}/explorer.cpp"
  "${SRC_DIR}/file.h"
  "${SRC_DIR}/file.cpp"
  "${SRC_DIR}/filetype.h"
  "${SRC_DIR}/filetype.cpp"
  "${SRC_DIR}/fs.h"
  "${SRC_DIR}/fs.cpp"
  "${SRC_DIR}/fs_monitor.h"
  "${SRC_DIR}/fs_monitor.cpp"
  "${SRC_DIR}/history.h"
  "${SRC_DIR}/json.h"
  "${SRC_DIR}/keymaps.cpp"
  "${SRC_DIR}/keyseq_manager.h"
  "${SRC_DIR}/keyseq_manager.cpp"
  "${SRC_DIR}/layout_manager.h"
  "${SRC_DIR}/layout_manager.cpp"
  "${SRC_DIR}/list_area.h"
  "${SRC_DIR}/logging.h"
  "${SRC_DIR}/logging.cpp"
  "${SRC_DIR}/mango_peel.h"
  "${SRC_DIR}/mango_peel.cpp"
  "${SRC_DIR}/mouse.h"
  "${SRC_DIR}/options.h"
  "${SRC_DIR}/options.cpp"
  "${SRC_DIR}/pos.h"
  "${SRC_DIR}/result.h"
  "${SRC_DIR}/search.h"
  "${SRC_DIR}/search.cpp"
  "${SRC_DIR}/selection.h"
  "${SRC_DIR}/selection.cpp"
  "${SRC_DIR}/signal_handler.h"
  "${SRC_DIR}/signal_handler.cpp"
  "${SRC_DIR}/str.h"
  "${SRC_DIR}/str.cpp"
  "${SRC_DIR}/subprocess.h"
  "${SRC_DIR}/subprocess.cpp"
  "${SRC_DIR}/syntax.h"
  "${SRC_DIR}/syntax.cpp"
  "${SRC_DIR}/os.h"
  "${SRC_DIR}/state.h"
  "${SRC_DIR}/state.cpp"
  "${SRC_DIR}/status_line.h"
  "${SRC_DIR}/status_line.cpp"
  "${SRC_DIR}/term.h"
  "${SRC_DIR}/term.cpp"
  "${SRC_DIR}/text_area.h"
  "${SRC_DIR}/text_area.cpp"
  "${SRC_DIR}/text_window.h"
  "${SRC_DIR}/text_window.cpp"
  "${SRC_DIR}/text_tree.h"
  "${SRC_DIR}/text_tree.cpp"
  "${SRC_DIR}/timer_manager.h"
  "${SRC_DIR}/timer_manager.cpp"
  "${SRC_DIR}/trie.h"
  "${SRC_DIR}/trie.cpp"
  "${SRC_DIR}/utf8.h"
  "${SRC_DIR}/utf8.cpp"
  "${SRC_DIR}/utils.h"
  "${SRC_DIR}/utils.cpp"
  "${SRC_DIR}/window.h"
  "${SRC_DIR}/window.cpp"
)

foreach(LANG IN LISTS TS_GRAMMAR_LANGS)
    set(BASE "${TS_GRAMMAR_DIR}/tree-sitter-${LANG}/src")

    list(APPEND TS_SOURCES "${BASE}/parser.c")

    if(EXISTS "${BASE}/scanner.c")
        list(APPEND TS_SOURCES "${BASE}/scanner.c")
    endif()

    if(EXISTS "${BASE}/scanner.cc")
        list(APPEND TS_SOURCES "${BASE}/scanner.cc")
    endif()
endforeach()
target_sources(charxed_lib PUBLIC ${TS_SOURCES})

target_link_libraries(
  charxed_lib
  PUBLIC
    ${TERMBOX2_LIB}
    Microsoft.GSL::GSL
    nlohmann_json::nlohmann_json
    tree-sitter
    utf8proc
    fmt::fmt
)
target_include_directories(
  charxed_lib
  PUBLIC
  "${SRC_DIR}"
  "${TERMBOX2_INCLUDE_DIR}"
  "${CMAKE_BINARY_DIR}"
)

target_compile_definitions(charxed_lib PUBLIC TB_OPT_ATTR_W=64 TB_OPT_EGC)

# charxed executable
add_executable(chx)
target_sources(
  chx PUBLIC
  "${SRC_DIR}/main.cpp"
)
target_link_libraries(chx PUBLIC charxed_lib)

# Tests
set(TEST_DIR "${CMAKE_SOURCE_DIR}/test")

add_executable(test)
target_sources(
  test PUBLIC
  "${TEST_DIR}/buffer_naive.h"
  "${TEST_DIR}/buffer_naive.cpp"
  "${TEST_DIR}/clipboard_test.cpp"
  "${TEST_DIR}/data_structure_test.cpp"
  "${TEST_DIR}/fs_test.cpp"
  "${TEST_DIR}/json_test.cpp"
  "${TEST_DIR}/logging_test.cpp"
  "${TEST_DIR}/lsp_test.cpp"
  "${TEST_DIR}/subprocess_test.cpp"
  "${TEST_DIR}/term_test.cpp"
  "${TEST_DIR}/text_tree_test.cpp"
  "${TEST_DIR}/unicode_test.cpp"
  "${TEST_DIR}/xxx_manager_test.cpp"
)
target_link_libraries(test PUBLIC charxed_lib Catch2::Catch2WithMain)

# Package
install(TARGETS chx RUNTIME DESTINATION bin)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/resource/" DESTINATION resource)

set(CPACK_PACKAGE_FILE_NAME "Charxed-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "ZIP")
include(CPack)
