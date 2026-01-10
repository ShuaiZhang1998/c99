#include "sema.h"

#include <unordered_set>
#include <vector>
#include <string>

namespace c99cc {

namespace {

using Scope = std::unordered_set<std::string>;
using ScopeStack = std::vector<Scope>;

static bool isDeclared(const ScopeStack& scopes, const std::string& name) {
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    if (it->count(name)) return true;
  }
  return false;
}

static void checkExprImpl(Diagnostics& diags, ScopeStack& scopes, const Expr& e);

static void checkStmtImpl(Diagnostics& diags, ScopeStack& scopes, const Stmt& s) {
  if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
    scopes.push_back({});
    for (const auto& st : b->stmts) checkStmtImpl(diags, scopes, *st);
    scopes.pop_back();
    return;
  }

  if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
    checkExprImpl(diags, scopes, *i->cond);
    checkStmtImpl(diags, scopes, *i->thenBranch);
    if (i->elseBranch) checkStmtImpl(diags, scopes, *i->elseBranch);
    return;
  }

  if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
    checkExprImpl(diags, scopes, *w->cond);
    checkStmtImpl(diags, scopes, *w->body);
    return;
  }

  if (auto* d = dynamic_cast<const DeclStmt*>(&s)) {
    auto& cur = scopes.back();
    if (cur.count(d->name)) {
      diags.error(d->nameLoc, "redefinition of '" + d->name + "'");
      return;
    }
    if (d->initExpr) checkExprImpl(diags, scopes, *d->initExpr);
    cur.insert(d->name);
    return;
  }

  if (auto* a = dynamic_cast<const AssignStmt*>(&s)) {
    checkExprImpl(diags, scopes, *a->valueExpr);
    if (!isDeclared(scopes, a->name)) {
      diags.error(a->nameLoc, "assignment to undeclared identifier '" + a->name + "'");
    }
    return;
  }

  if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
    checkExprImpl(diags, scopes, *r->valueExpr);
    return;
  }
}

static void checkExprImpl(Diagnostics& diags, ScopeStack& scopes, const Expr& e) {
  if (dynamic_cast<const IntLiteralExpr*>(&e)) return;

  if (auto* vr = dynamic_cast<const VarRefExpr*>(&e)) {
    if (!isDeclared(scopes, vr->name)) {
      diags.error(vr->loc, "use of undeclared identifier '" + vr->name + "'");
    }
    return;
  }

  if (auto* asn = dynamic_cast<const AssignExpr*>(&e)) {
    // RHS always checked
    checkExprImpl(diags, scopes, *asn->rhs);
    // LHS name must already exist (assignment expression cannot declare)
    if (!isDeclared(scopes, asn->name)) {
      diags.error(asn->nameLoc, "assignment to undeclared identifier '" + asn->name + "'");
    }
    return;
  }

  if (auto* un = dynamic_cast<const UnaryExpr*>(&e)) {
    checkExprImpl(diags, scopes, *un->operand);
    return;
  }

  if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
    checkExprImpl(diags, scopes, *bin->lhs);
    checkExprImpl(diags, scopes, *bin->rhs);
    return;
  }
}

} // namespace

bool Sema::run(const AstTranslationUnit& tu) {
  ScopeStack scopes;
  scopes.push_back({});
  for (const auto& st : tu.body) checkStmtImpl(diags_, scopes, *st);
  return !diags_.hasError();
}

} // namespace c99cc
