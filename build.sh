#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

CXX=clang++-20
LLVM_CONFIG=llvm-config-20

CXXFLAGS=$($LLVM_CONFIG --cxxflags)
LDFLAGS=$($LLVM_CONFIG --ldflags)
SYSLIBS=$($LLVM_CONFIG --system-libs)
LLVMLIBS=$($LLVM_CONFIG --libs)

CLANG_LIBS="-lclangTooling -lclangToolingRefactoring -lclangToolingCore \
-lclangFrontend -lclangDriver -lclangSerialization -lclangParse \
-lclangSema -lclangAnalysis -lclangAST -lclangASTMatchers -lclangRewrite \
-lclangEdit -lclangLex -lclangBasic -lclangSupport \
-lclangFormat -lclangAPINotes -lclangToolingInclusions -lclangToolingInclusionsStdlib"

$CXX -std=c++17 -fno-rtti $CXXFLAGS path_format_fixer.cpp -o path_format_fixer \
    $LDFLAGS \
    -Wl,--start-group $CLANG_LIBS -Wl,--end-group \
    $LLVMLIBS $SYSLIBS

echo "Built ./path_format_fixer"
