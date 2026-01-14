#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace c99cc {

class Preprocessor {
public:
  explicit Preprocessor(
      std::vector<std::string> includePaths = {},
      std::vector<std::string> systemIncludePaths = {});
  std::optional<std::string> run(const std::string& path, const std::string& source);
  void addIncludePath(const std::string& path);
  void addSystemIncludePath(const std::string& path);

private:
  struct Macro {
    bool functionLike = false;
    bool variadic = false;
    std::vector<std::string> params;
    std::string body;
  };

  struct IfState {
    bool parentActive = true;
    bool condition = true;
    bool inElse = false;
    bool taken = false;
  };

  std::unordered_map<std::string, Macro> macros_;
  std::vector<std::string> includePaths_;
  std::vector<std::string> systemIncludePaths_;
  std::vector<std::string> errors_;

  bool processFile(const std::string& path, const std::string& source, std::string& out);
  bool processLines(const std::string& path, const std::string& source, std::string& out);
  bool handleDirective(
      const std::string& path, int line, const std::string& lineText,
      std::vector<IfState>& ifs, std::string& out);

  bool evalIfExpr(const std::string& expr, bool& out, std::string& err);
  std::string expandLine(const std::string& line);
  std::string expandText(
      const std::string& text, std::unordered_map<std::string, bool>& expanding, int depth);

  bool report(const std::string& path, int line, int col, const std::string& msg);
  bool readFile(const std::string& path, std::string& out);
  std::string dirName(const std::string& path);
  bool resolveInclude(
      const std::string& header,
      bool isSystem,
      const std::string& currentPath,
      std::string& resolvedPath,
      std::string& content);
};

} // namespace c99cc
