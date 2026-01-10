#pragma once
#include <string>
#include <vector>

namespace c99cc {

struct SourceLocation {
  size_t offset = 0; // buffer offset
  int line = 1;
  int col  = 1;
};

struct Diagnostic {
  enum class Level { Error, Warning, Note };
  Level level;
  std::string message;
  SourceLocation loc;
};

class Diagnostics {
public:
  void error(const SourceLocation& loc, std::string msg);
  bool hasError() const { return has_error_; }
  void printAll(const std::string& filename, const std::string& source) const;

private:
  bool has_error_ = false;
  std::vector<Diagnostic> diags_;
};

} // namespace c99cc
