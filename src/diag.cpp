#include "diag.h"
#include <iostream>
#include <algorithm>

namespace c99cc {

static const char* levelName(Diagnostic::Level lv) {
  switch (lv) {
    case Diagnostic::Level::Error: return "error";
    case Diagnostic::Level::Warning: return "warning";
    case Diagnostic::Level::Note: return "note";
  }
  return "unknown";
}

void Diagnostics::error(const SourceLocation& loc, std::string msg) {
  has_error_ = true;
  diags_.push_back(Diagnostic{Diagnostic::Level::Error, std::move(msg), loc});
}

static std::string getLineText(const std::string& src, int line) {
  int cur = 1;
  size_t start = 0;
  for (size_t i = 0; i < src.size(); i++) {
    if (src[i] == '\n') {
      if (cur == line) return src.substr(start, i - start);
      cur++;
      start = i + 1;
    }
  }
  if (cur == line) return src.substr(start);
  return "";
}

void Diagnostics::printAll(const std::string& filename, const std::string& source) const {
  for (const auto& d : diags_) {
    std::cerr << filename << ":" << d.loc.line << ":" << d.loc.col
              << ": " << levelName(d.level) << ": " << d.message << "\n";

    auto lineText = getLineText(source, d.loc.line);
    if (!lineText.empty()) {
      std::cerr << "  " << lineText << "\n";
      std::cerr << "  " << std::string(std::max(0, d.loc.col - 1), ' ') << "^\n";
    }
  }
}

} // namespace c99cc
