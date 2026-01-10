#pragma once
#include "parser.h"
#include "diag.h"

namespace c99cc {

class Sema {
public:
  explicit Sema(Diagnostics& diags) : diags_(diags) {}

  bool run(const AstTranslationUnit& tu);

private:
  void checkStmt(const Stmt& s);
  void checkExpr(const Expr& e);

  Diagnostics& diags_;
};

} // namespace c99cc
