#pragma once
#include "diag.h"
#include "parser.h"

namespace c99cc {

class Sema {
public:
  explicit Sema(Diagnostics& diags) : diags_(diags) {}
  bool run(const AstTranslationUnit& tu);

private:
  Diagnostics& diags_;
};

} // namespace c99cc
