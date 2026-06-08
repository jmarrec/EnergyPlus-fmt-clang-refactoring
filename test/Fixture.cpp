#include "Fixture.hpp"

#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/StringRef.h"

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

std::string runOnCode(llvm::StringRef Code, const std::vector<std::string>& FormatFunctionNames) {
  std::map<std::string, Replacements> FileToReplaces;
  path_format_fixer::PathFormatCallback Callback(FileToReplaces);

  MatchFinder Finder;
  Finder.addMatcher(path_format_fixer::makeMatcher(FormatFunctionNames), &Callback);

  std::vector<std::string> Args = {
    "-std=c++20",
#if defined(PATH_FORMAT_FIXER_TEST_CLANG_RESOURCE_DIR) && defined(PATH_FORMAT_FIXER_TEST_CLANG_LIBCXX_DIR)
    // See test/CMakeLists.txt: steer this from-scratch compilation away from the
    // system SDK's bundled libc++ and onto the one shipped by the compiler that's
    // actually running it, so header versions stay consistent.
    "-nostdinc++",
    "-resource-dir",
    PATH_FORMAT_FIXER_TEST_CLANG_RESOURCE_DIR,
    "-isystem",
    PATH_FORMAT_FIXER_TEST_CLANG_LIBCXX_DIR,
#endif
#ifdef PATH_FORMAT_FIXER_TEST_FMT_INCLUDE_DIR
    // The wrapped snippets `#include <fmt/core.h>`; point at where Conan put it.
    "-isystem",
    PATH_FORMAT_FIXER_TEST_FMT_INCLUDE_DIR,
#endif
  };

  std::string FileName = "input.cc";
  bool Ran = runToolOnCodeWithArgs(newFrontendActionFactory(&Finder)->create(), Code, Args, FileName);
  if (!Ran || FileToReplaces.empty()) {
    return Code.str();
  }

  auto It = FileToReplaces.find(FileName);
  if (It == FileToReplaces.end()) {
    It = FileToReplaces.begin();
  }

  llvm::Expected<std::string> Result = applyAllReplacements(Code, It->second);
  if (!Result) {
    llvm::consumeError(Result.takeError());
    return Code.str();
  }
  return *Result;
}
