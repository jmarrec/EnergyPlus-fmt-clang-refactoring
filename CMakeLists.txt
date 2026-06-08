cmake_minimum_required(VERSION 3.20)
project(path_format_fixer CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Clang REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

add_executable(path_format_fixer path_format_fixer.cpp)

target_include_directories(path_format_fixer PRIVATE ${LLVM_INCLUDE_DIRS} ${CLANG_INCLUDE_DIRS})
target_compile_definitions(path_format_fixer PRIVATE ${LLVM_DEFINITIONS})

if(NOT LLVM_ENABLE_RTTI)
    target_compile_options(path_format_fixer PRIVATE -fno-rtti)
endif()

target_link_libraries(path_format_fixer
    PRIVATE
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
