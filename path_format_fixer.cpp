#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

#include <cctype>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace clang;
using namespace clang::ast_matchers;
using namespace clang::tooling;

namespace {

llvm::cl::OptionCategory ToolCategory("path-format-fixer options");

llvm::cl::opt<std::string> FormatFunctionName(
    "format-function",
    llvm::cl::desc("Qualified name of the format function to match (default: EnergyPlus::format)"),
    llvm::cl::init("EnergyPlus::format"), llvm::cl::cat(ToolCategory));

// A `{...}` replacement field found in a format string's source spelling.
struct Field {
  size_t Offset;     // byte offset of '{' within the literal's source spelling
  size_t Length;     // length including both braces
  std::string Inner; // text between '{' and '}'
  int ArgIndex;      // 0-based variadic-argument index this field refers to
};

// Scans `Text` (the raw source spelling of a format-string literal, including
// quotes) for `{...}` replacement fields, skipping `{{`/`}}` brace escapes.
// Nested replacement fields inside a format spec are not handled (such fields
// are returned with ArgIndex == -1, and parsing aborts with std::nullopt if
// they're malformed).
//
// Returns, for each field in order, the field text/location plus which
// (0-based) variadic argument it refers to: either the explicit leading index
// (`{2:...}`) or, for auto-numbered fields (`{}`/`{:...}`), the next index in
// sequence. Mixing the two styles is invalid std::format anyway, so treating
// them uniformly here is fine.
std::optional<std::vector<Field>> scanReplacementFields(StringRef Text) {
  std::vector<Field> Fields;
  unsigned AutoIndex = 0;
  for (size_t I = 0; I < Text.size();) {
    char C = Text[I];
    if (C == '{') {
      if (I + 1 < Text.size() && Text[I + 1] == '{') {
        I += 2;
        continue;
      }
      size_t Start = I;
      size_t J = I + 1;
      while (J < Text.size() && Text[J] != '}')
        ++J;
      if (J >= Text.size())
        return std::nullopt;
      std::string Inner = Text.substr(Start + 1, J - Start - 1).str();

      // Leading digits, if any, are an explicit argument index.
      size_t NumEnd = 0;
      while (NumEnd < Inner.size() && isdigit(static_cast<unsigned char>(Inner[NumEnd])))
        ++NumEnd;

      int ArgIndex;
      if (NumEnd > 0)
        ArgIndex = std::stoi(Inner.substr(0, NumEnd));
      else
        ArgIndex = static_cast<int>(AutoIndex++);

      Fields.push_back(Field{Start, J - Start + 1, std::move(Inner), ArgIndex});
      I = J + 1;
    } else if (C == '}') {
      if (I + 1 < Text.size() && Text[I + 1] == '}') {
        I += 2;
        continue;
      }
      ++I;
    } else {
      ++I;
    }
  }
  return Fields;
}

class PathFormatCallback : public MatchFinder::MatchCallback {
public:
  explicit PathFormatCallback(std::map<std::string, Replacements> &FileToReplaces)
      : FileToReplaces(FileToReplaces) {}

  void run(const MatchFinder::MatchResult &Result) override {
    const auto *FormatCall = Result.Nodes.getNodeAs<CallExpr>("formatCall");
    const auto *PathCall = Result.Nodes.getNodeAs<CXXMemberCallExpr>("pathCall");
    if (!FormatCall || !PathCall)
      return;

    const SourceManager &SM = *Result.SourceManager;
    const LangOptions &LangOpts = Result.Context->getLangOpts();

    if (FormatCall->getNumArgs() < 2)
      return;

    const Expr *FmtArg = FormatCall->getArg(0)->IgnoreUnlessSpelledInSource();
    const auto *FmtLiteral = dyn_cast<StringLiteral>(FmtArg);
    if (!FmtLiteral) {
      diag(SM, FormatCall->getExprLoc(), "format string is not a simple string literal");
      return;
    }
    if (FmtLiteral->getNumConcatenated() != 1) {
      diag(SM, FormatCall->getExprLoc(), "concatenated format string literal");
      return;
    }

    // Which variadic argument (0-based -> matches {} auto-numbering) holds PathCall?
    int ArgIndex = -1;
    SourceRange CallRange = PathCall->getSourceRange();
    for (unsigned I = 1; I < FormatCall->getNumArgs(); ++I) {
      SourceRange ArgRange = FormatCall->getArg(I)->getSourceRange();
      if (!SM.isBeforeInTranslationUnit(CallRange.getBegin(), ArgRange.getBegin()) &&
          !SM.isBeforeInTranslationUnit(ArgRange.getEnd(), CallRange.getEnd())) {
        ArgIndex = static_cast<int>(I - 1);
        break;
      }
    }
    if (ArgIndex < 0) {
      diag(SM, PathCall->getExprLoc(), "could not map path call to a format argument");
      return;
    }

    bool IsGeneric = PathCall->getMethodDecl()->getName() == "generic_string";

    CharSourceRange FmtTokenRange = CharSourceRange::getTokenRange(FmtLiteral->getSourceRange());
    StringRef FmtSourceText = Lexer::getSourceText(FmtTokenRange, SM, LangOpts);

    if (IsGeneric) {
      auto MaybeFields = scanReplacementFields(FmtSourceText);
      if (!MaybeFields) {
        diag(SM, FmtLiteral->getExprLoc(), "could not parse replacement fields in format string");
        return;
      }
      const Field *Found = nullptr;
      for (const Field &Candidate : *MaybeFields) {
        if (Candidate.ArgIndex == ArgIndex) {
          Found = &Candidate;
          break;
        }
      }
      if (!Found) {
        diag(SM, FmtLiteral->getExprLoc(),
             "could not find a replacement field referring to argument #" +
                 std::to_string(ArgIndex) + " in format string");
        return;
      }
      const Field &F = *Found;
      if (F.Inner.find(':') != std::string::npos) {
        diag(SM, FmtLiteral->getExprLoc(),
             "replacement field #" + std::to_string(ArgIndex) +
                 " already has a format spec ('" + F.Inner + "')");
        return;
      }
      std::string NewField = "{" + F.Inner + ":g}";
      SourceLocation FieldStart = FmtLiteral->getBeginLoc().getLocWithOffset(F.Offset);
      addReplacement(SM,
                     CharSourceRange::getCharRange(FieldStart, FieldStart.getLocWithOffset(F.Length)),
                     NewField);
    }

    // <expr>.string() / <expr>.generic_string()  ->  <expr>
    const Expr *Base = PathCall->getImplicitObjectArgument()->IgnoreParenImpCasts();
    StringRef BaseText =
        Lexer::getSourceText(CharSourceRange::getTokenRange(Base->getSourceRange()), SM, LangOpts);

    addReplacement(SM, CharSourceRange::getTokenRange(PathCall->getSourceRange()), BaseText);
  }

private:
  void diag(const SourceManager &SM, SourceLocation Loc, const std::string &Msg) {
    llvm::errs() << "skip: " << Msg << " at " << Loc.printToString(SM) << "\n";
  }

  void addReplacement(const SourceManager &SM, CharSourceRange Range, StringRef NewText) {
    Replacement Repl(SM, Range, NewText);
    if (llvm::Error Err = FileToReplaces[std::string(Repl.getFilePath())].add(Repl))
      llvm::errs() << "error adding replacement: " << llvm::toString(std::move(Err)) << "\n";
  }

  std::map<std::string, Replacements> &FileToReplaces;
};

} // namespace

int main(int argc, const char **argv) {
  auto OptionsParser = CommonOptionsParser::create(argc, argv, ToolCategory);
  if (!OptionsParser) {
    llvm::errs() << llvm::toString(OptionsParser.takeError()) << "\n";
    return 1;
  }

  RefactoringTool Tool(OptionsParser->getCompilations(), OptionsParser->getSourcePathList());

  auto PathToStringCall =
      cxxMemberCallExpr(callee(cxxMethodDecl(hasAnyName("string", "generic_string"))),
                        on(hasType(cxxRecordDecl(hasName("::std::filesystem::path")))))
          .bind("pathCall");

  auto Matcher = callExpr(callee(functionDecl(hasName(FormatFunctionName))),
                          hasAnyArgument(expr(anyOf(PathToStringCall, hasDescendant(PathToStringCall)))))
                     .bind("formatCall");

  PathFormatCallback Callback(Tool.getReplacements());
  MatchFinder Finder;
  Finder.addMatcher(Matcher, &Callback);

  return Tool.runAndSave(newFrontendActionFactory(&Finder).get());
}
