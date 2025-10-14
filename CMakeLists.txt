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
    COMMAND make libtermbox2.a
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
set(GSL_INCLUDE_DIR "${THIRD_PARTY_DIR}/GSL/include")

FetchContent_MakeAvailable(termbox2 GSL)

# TODO: use ICU and support grapheme cluster
# # ICU
# find_package(ICU REQUIRED COMPONENTS uc i18n data)

# Mango library
set(SRC_DIR "${CMAKE_SOURCE_DIR}/src")
add_library(mango_lib)
add_dependencies(mango_lib build_termbox2)
target_sources(
  mango_lib PUBLIC
  "${SRC_DIR}/buffer.h"
  "${SRC_DIR}/buffer.cpp"
  "${SRC_DIR}/cmd_parse.h"
  "${SRC_DIR}/cmd_parse.cpp"
  "${SRC_DIR}/coding.h"
  "${SRC_DIR}/coding.cpp"
  "${SRC_DIR}/common.h"
  "${SRC_DIR}/cursor.h"
  "${SRC_DIR}/cursor.cpp"
  "${SRC_DIR}/editor.h"
  "${SRC_DIR}/editor.cpp"
  "${SRC_DIR}/exception.h"
  "${SRC_DIR}/exception.cpp"
  "${SRC_DIR}/frame.h"
  "${SRC_DIR}/frame.cpp"
  "${SRC_DIR}/logging.h"
  "${SRC_DIR}/logging.cpp"
  "${SRC_DIR}/options.h"
  "${SRC_DIR}/options.cpp"
  "${SRC_DIR}/result.h"
  "${SRC_DIR}/signal_handler.h"
  "${SRC_DIR}/signal_handler.cpp"
  "${SRC_DIR}/status_line.h"
  "${SRC_DIR}/status_line.cpp"
  "${SRC_DIR}/term.h"
  "${SRC_DIR}/term.cpp"
  "${SRC_DIR}/utils.h"
  "${SRC_DIR}/utils.cpp"
  "${SRC_DIR}/window.h"
  "${SRC_DIR}/window.cpp"
)
target_link_libraries(mango_lib PUBLIC ${TERMBOX2_LIB} PUBLIC Microsoft.GSL::GSL)
target_include_directories(mango_lib PUBLIC "${SRC_DIR}" "${TERMBOX2_INCLUDE_DIR}" "${GSL_INCLUDE_DIR}")

# mango executable
add_executable(mango)
target_sources(
  mango PUBLIC
  "${SRC_DIR}/main.cpp"
)
target_link_libraries(mango PUBLIC mango_lib)

# Tests
set(TEST_DIR "${CMAKE_SOURCE_DIR}/test")

add_executable(logging_test "${TEST_DIR}/logging_test.cpp")
target_link_libraries(logging_test PUBLIC mango_lib)