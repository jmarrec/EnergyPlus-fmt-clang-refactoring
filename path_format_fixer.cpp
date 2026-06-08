#include "PathFormatFixer.hpp"

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace clang::tooling;

namespace {

llvm::cl::OptionCategory ToolCategory("path-format-fixer options");

llvm::cl::opt<std::string> FormatFunctionName(
    "format-function",
    llvm::cl::desc("Qualified name of the format function to match (default: EnergyPlus::format)"),
    llvm::cl::init("EnergyPlus::format"), llvm::cl::cat(ToolCategory));

} // namespace

int main(int argc, const char **argv) {
  auto OptionsParser = CommonOptionsParser::create(argc, argv, ToolCategory);
  if (!OptionsParser) {
    llvm::errs() << llvm::toString(OptionsParser.takeError()) << "\n";
    return 1;
  }

  RefactoringTool Tool(OptionsParser->getCompilations(), OptionsParser->getSourcePathList());

  path_format_fixer::PathFormatCallback Callback(Tool.getReplacements());
  ast_matchers::MatchFinder Finder;
  Finder.addMatcher(path_format_fixer::makeMatcher(FormatFunctionName), &Callback);

  return Tool.runAndSave(newFrontendActionFactory(&Finder).get());
}
