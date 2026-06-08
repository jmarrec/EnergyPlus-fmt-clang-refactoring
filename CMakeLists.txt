cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Do not enable compiler specific extensions, for eg on GCC use -std=c++1z (=c++17) and not -std=gnu++17
set(CMAKE_CXX_EXTENSIONS OFF)

project(path_format_fixer CXX C)

###############################################################################
#                         C M A K E    C O N T R O L                          #
###############################################################################

# High level project configuration
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/Products")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/Products")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/Products")

set(LIBRARY_SEARCH_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Release" "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Debug")

find_package(Clang REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

set(CLANG_TOOLING_LIBS
    clangTooling
    clangToolingRefactoring
    clangToolingCore
    clangFrontend
    clangDriver
    clangSerialization
    clangParse
    clangSema
    clangAnalysis
    clangAST
    clangASTMatchers
    clangRewrite
    clangEdit
    clangLex
    clangBasic
    clangSupport
    clangFormat
    clangAPINotes
    clangToolingInclusions
    clangToolingInclusionsStdlib
)

add_library(llvm_clang_includes INTERFACE)
if(NOT LLVM_ENABLE_RTTI)
  target_compile_options(llvm_clang_includes INTERFACE -fno-rtti)
endif()
# Note: deliberately NOT target_include_directories()/-I/-isystem here.
# /usr/lib/llvm-20/include ships its own cxxabi.h (meant to pair with
# libc++abi); when it's found ahead of libstdc++'s, any TU that ends up
# pulling in both (e.g. gtest's <gtest/internal/gtest-type-util.h> includes
# <cxxabi.h> for demangling, while libstdc++'s <exception> drags in
# bits/cxxabi_init_exception.h) fails with conflicting declarations of
# __cxa_init_primary_exception. -idirafter searches these dirs only after
# the compiler's builtin system include paths, so libstdc++'s cxxabi.h is
# found first while llvm/... and clang/... headers still resolve correctly.
foreach(dir IN LISTS LLVM_INCLUDE_DIRS CLANG_INCLUDE_DIRS)
  target_compile_options(llvm_clang_includes INTERFACE -idirafter${dir})
endforeach()

add_library(path_format_fixer_lib STATIC
  PathFormatFixer.cpp
  PathFormatFixer.hpp
)
target_compile_definitions(path_format_fixer_lib PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(path_format_fixer_lib
  PRIVATE
    ${CLANG_TOOLING_LIBS}
    llvm_clang_includes
)

add_executable(path_format_fixer path_format_fixer.cpp)
target_compile_definitions(path_format_fixer PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(path_format_fixer PRIVATE path_format_fixer_lib llvm_clang_includes)

if(BUILD_TESTING)
  # Conan drops its generated Find/*Config.cmake modules in the build dir;
  # make sure they're found ahead of CMake's bundled FindGTest.cmake.
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")
  list(APPEND CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}")

  find_package(fmt REQUIRED)
  find_package(GTest REQUIRED)

  enable_testing()
  include(GoogleTest)

  add_subdirectory(test)
endif()
