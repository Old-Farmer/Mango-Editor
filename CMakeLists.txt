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
set(FETCHCONTENT_QUIET OFF)

# third-party
set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third-party")

# termbox2
FetchContent_Declare(
    termbox2
    GIT_REPOSITORY "https://github.com/Old-Farmer/termbox2.git"
    GIT_TAG dev
    GIT_SHALLOW ON
    GIT_PROGRESS ON
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
FetchContent_Populate(termbox2)

# utf8proc
FetchContent_Declare(
    utf8proc
    GIT_REPOSITORY "https://github.com/JuliaStrings/utf8proc.git"
    GIT_TAG master
    GIT_SHALLOW ON
    GIT_PROGRESS ON
    SOURCE_DIR "${THIRD_PARTY_DIR}/utf8proc"
)
set(UTF8PROC_LIB "${THIRD_PARTY_DIR}/utf8proc/libutf8proc.a")
set(UTF8PROC_INCLUDE_DIR "${THIRD_PARTY_DIR}/utf8proc")
add_custom_target(build_utf8proc
    COMMAND make
    WORKING_DIRECTORY "${THIRD_PARTY_DIR}/utf8proc"
    COMMENT "Building utf8proc library"
    BYPRODUCTS "${UTF8PROC_LIB}"
)
FetchContent_Populate(utf8proc)

# GSL
FetchContent_Declare(
    GSL
    GIT_REPOSITORY "https://github.com/microsoft/GSL"
    GIT_TAG "v4.2.0"
    GIT_SHALLOW ON
    GIT_PROGRESS ON
    SOURCE_DIR "${THIRD_PARTY_DIR}/GSL"
)
FetchContent_MakeAvailable(GSL)

# json
FetchContent_Declare(json
    URL "https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz"
    SOURCE_DIR "${THIRD_PARTY_DIR}/json"
)
FetchContent_MakeAvailable(json)

# catch2
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.8.1
    GIT_SHALLOW ON
    GIT_PROGRESS ON
    SOURCE_DIR "${THIRD_PARTY_DIR}/Catch2"
)
FetchContent_MakeAvailable(Catch2)

# tree-sitter
FetchContent_Declare(
    treesitter
    GIT_REPOSITORY "https://github.com/tree-sitter/tree-sitter.git"
    GIT_TAG master
    GIT_SHALLOW ON
    GIT_PROGRESS ON
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
FetchContent_Populate(treesitter)

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
set(TS_GRAMMAR_LANGS
    c
    cpp
    json
)
set(TS_GRAMMAR_DIR "${THIRD_PARTY_DIR}/tree-sitter-grammars")
foreach(LANG IN LISTS TS_GRAMMAR_LANGS)
    set(URL "https://github.com/tree-sitter/tree-sitter-${LANG}.git")
    set(NAME ts_grammar_${LANG})
    FetchContent_Declare(
        ${NAME}
        GIT_REPOSITORY ${URL}
        GIT_TAG master
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        SOURCE_DIR "${TS_GRAMMAR_DIR}/tree-sitter-${LANG}"
    ) 
    # Use populate Instead of MakeAvailabe to avoid target conflicts.
    FetchContent_Populate(${NAME})
endforeach()

# TODO: use ICU and support grapheme cluster
# # ICU
# find_package(ICU REQUIRED COMPONENTS uc i18n data)

# Mango library
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
add_library(mango_lib STATIC)
add_dependencies(mango_lib build_termbox2 build_treesitter build_utf8proc)
target_sources(
  mango_lib PUBLIC
  "${SRC_DIR}/buffer_manager.h"
  "${SRC_DIR}/buffer_manager.cpp"
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
  "${SRC_DIR}/completer.h"
  "${SRC_DIR}/completer.cpp"
  "${SRC_DIR}/cursor.h"
  "${SRC_DIR}/cursor.cpp"
  "${SRC_DIR}/editor.h"
  "${SRC_DIR}/editor.cpp"
  "${SRC_DIR}/exception.h"
  "${SRC_DIR}/exception.cpp"
  "${SRC_DIR}/file.h"
  "${SRC_DIR}/file.cpp"
  "${SRC_DIR}/filetype.h"
  "${SRC_DIR}/filetype.cpp"
  "${SRC_DIR}/frame.h"
  "${SRC_DIR}/frame.cpp"
  "${SRC_DIR}/fs.h"
  "${SRC_DIR}/fs.cpp"
  "${SRC_DIR}/json.h"
  "${SRC_DIR}/keyseq_manager.h"
  "${SRC_DIR}/keyseq_manager.cpp"
  "${SRC_DIR}/logging.h"
  "${SRC_DIR}/logging.cpp"
  "${SRC_DIR}/mango_peel.h"
  "${SRC_DIR}/mango_peel.cpp"
  "${SRC_DIR}/mouse.h"
  "${SRC_DIR}/options.h"
  "${SRC_DIR}/options.cpp"
  "${SRC_DIR}/pos.h"
  "${SRC_DIR}/result.h"
  "${SRC_DIR}/selection.h"
  "${SRC_DIR}/selection.cpp"
  "${SRC_DIR}/signal_handler.h"
  "${SRC_DIR}/signal_handler.cpp"
  "${SRC_DIR}/syntax.h"
  "${SRC_DIR}/syntax.cpp"
  "${SRC_DIR}/os.h"
  "${SRC_DIR}/state.h"
  "${SRC_DIR}/state.cpp"
  "${SRC_DIR}/status_line.h"
  "${SRC_DIR}/status_line.cpp"
  "${SRC_DIR}/term.h"
  "${SRC_DIR}/term.cpp"
  "${SRC_DIR}/trie.h"
  "${SRC_DIR}/trie.cpp"
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
target_sources(mango_lib PUBLIC ${TS_SOURCES})

target_link_libraries(
  mango_lib
  PUBLIC
    ${TERMBOX2_LIB}
    Microsoft.GSL::GSL 
    nlohmann_json::nlohmann_json
    ${TREESITTER_LIB}
    ${UTF8PROC_LIB}
)
target_include_directories(
  mango_lib 
  PUBLIC
  "${SRC_DIR}"
  "${TERMBOX2_INCLUDE_DIR}"
  "${TREESITTER_INCLUDE_DIR}"
  "${UTF8PROC_INCLUDE_DIR}"
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
  "${TEST_DIR}/data_structure_test.cpp"
  "${TEST_DIR}/json_test.cpp"
  "${TEST_DIR}/logging_test.cpp"
  "${TEST_DIR}/term_test.cpp"
  "${TEST_DIR}/unicode_test.cpp"
  "${TEST_DIR}/utils_test.cpp"
  "${TEST_DIR}/xxx_manager_test.cpp"
)
target_link_libraries(test PUBLIC mango_lib Catch2::Catch2WithMain)

# Package
install(TARGETS mgo RUNTIME DESTINATION bin)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/resource/" DESTINATION resource)

set(CPACK_PACKAGE_FILE_NAME "mango-editor-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "ZIP")
include(CPack)
