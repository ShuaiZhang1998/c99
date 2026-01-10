#include "sema.h"
#include <unordered_set>

namespace c99cc {

namespace {
struct SymTable {
  std::unordered_set<std::string> declared;
};
}

static void checkExprImpl(Diagnostics& diags, SymTable& syms, const Expr& e) {
  if (dynamic_cast<const IntLiteralExpr*>(&e)) return;

  if (auto* vr = dynamic_cast<const VarRefExpr*>(&e)) {
    if (!syms.declared.count(vr->name)) {
      diags.error(vr->loc, "use of undeclared identifier '" + vr->name + "'");
    }
    return;
  }

  if (auto* un = dynamic_cast<const UnaryExpr*>(&e)) {
    checkExprImpl(diags, syms, *un->operand);
    return;
  }

  if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
    checkExprImpl(diags, syms, *bin->lhs);
    checkExprImpl(diags, syms, *bin->rhs);
    return;
  }
}

static void checkStmtImpl(Diagnostics& diags, SymTable& syms, const Stmt& s) {
  if (auto* d = dynamic_cast<const DeclStmt*>(&s)) {
    if (syms.declared.count(d->name)) {
      diags.error(d->nameLoc, "redefinition of '" + d->name + "'");
      return;
    }

    // initializer 不能引用正在声明的变量（此处尚未插入）
    if (d->initExpr) {
      checkExprImpl(diags, syms, *d->initExpr);
    }

    syms.declared.insert(d->name);
    return;
  }

  if (auto* a = dynamic_cast<const AssignStmt*>(&s)) {
    checkExprImpl(diags, syms, *a->valueExpr);
    if (!syms.declared.count(a->name)) {
      diags.error(a->nameLoc, "assignment to undeclared identifier '" + a->name + "'");
    }
    return;
  }

  if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
    checkExprImpl(diags, syms, *r->valueExpr);
    return;
  }

  if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
    for (const auto& st : b->stmts) checkStmtImpl(diags, syms, *st);
    return;
  }

  if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
    checkExprImpl(diags, syms, *i->cond);
    checkStmtImpl(diags, syms, *i->thenBranch);
    if (i->elseBranch) checkStmtImpl(diags, syms, *i->elseBranch);
    return;
  }

  if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
    checkExprImpl(diags, syms, *w->cond);
    checkStmtImpl(diags, syms, *w->body);
    return;
}
}

bool Sema::run(const AstTranslationUnit& tu) {
  SymTable syms;
  for (const auto& st : tu.body) {
    checkStmtImpl(diags_, syms, *st);
  }
  return !diags_.hasError();
}

} // namespace c99cc
