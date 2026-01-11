#include "sema.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace c99cc {

namespace {

using Scope = std::unordered_set<std::string>;
using ScopeStack = std::vector<Scope>;
using FnSigMap = std::unordered_map<std::string, size_t>; // name -> arity

static bool isDeclared(const ScopeStack& scopes, const std::string& name) {
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    if (it->count(name)) return true;
  }
  return false;
}

static void checkExprImpl(Diagnostics& diags, ScopeStack& scopes, const FnSigMap& fns, const Expr& e);

static void checkStmtImpl(
    Diagnostics& diags,
    ScopeStack& scopes,
    const FnSigMap& fns,
    int loopDepth,
    const Stmt& s) {
  if (auto* blk = dynamic_cast<const BlockStmt*>(&s)) {
    scopes.push_back({});
    for (const auto& st : blk->stmts) checkStmtImpl(diags, scopes, fns, loopDepth, *st);
    scopes.pop_back();
    return;
  }

  if (auto* iff = dynamic_cast<const IfStmt*>(&s)) {
    checkExprImpl(diags, scopes, fns, *iff->cond);
    checkStmtImpl(diags, scopes, fns, loopDepth, *iff->thenBranch);
    if (iff->elseBranch) checkStmtImpl(diags, scopes, fns, loopDepth, *iff->elseBranch);
    return;
  }

  if (auto* wh = dynamic_cast<const WhileStmt*>(&s)) {
    checkExprImpl(diags, scopes, fns, *wh->cond);
    checkStmtImpl(diags, scopes, fns, loopDepth + 1, *wh->body);
    return;
  }

  if (auto* dw = dynamic_cast<const DoWhileStmt*>(&s)) {
    checkStmtImpl(diags, scopes, fns, loopDepth + 1, *dw->body);
    checkExprImpl(diags, scopes, fns, *dw->cond);
    return;
  }

  if (auto* fo = dynamic_cast<const ForStmt*>(&s)) {
    // for introduces a new scope for init declarations (matches your existing behavior/tests)
    scopes.push_back({});

    if (fo->init) checkStmtImpl(diags, scopes, fns, loopDepth, *fo->init);
    if (fo->cond) checkExprImpl(diags, scopes, fns, *fo->cond);
    if (fo->inc)  checkExprImpl(diags, scopes, fns, *fo->inc);

    checkStmtImpl(diags, scopes, fns, loopDepth + 1, *fo->body);

    scopes.pop_back();
    return;
  }

  if (auto* br = dynamic_cast<const BreakStmt*>(&s)) {
    if (loopDepth <= 0) diags.error(br->loc, "break statement not within loop");
    return;
  }

  if (auto* co = dynamic_cast<const ContinueStmt*>(&s)) {
    if (loopDepth <= 0) diags.error(co->loc, "continue statement not within loop");
    return;
  }

  if (auto* decl = dynamic_cast<const DeclStmt*>(&s)) {
    // "redefinition" should be checked in current scope only
    auto& cur = scopes.back();
    if (cur.count(decl->name)) {
      diags.error(decl->nameLoc, "redefinition of '" + decl->name + "'");
      return;
    }

    // initializer cannot reference the variable being declared:
    // mimic your original rule by temporarily checking without inserting the name.
    if (decl->initExpr) {
      checkExprImpl(diags, scopes, fns, *decl->initExpr);
      // if initExpr references decl->name, checkExprImpl will see it as undeclared (good)
    }

    cur.insert(decl->name);
    return;
  }

  if (auto* as = dynamic_cast<const AssignStmt*>(&s)) {
    // you may still have AssignStmt in AST; support it
    checkExprImpl(diags, scopes, fns, *as->valueExpr);
    if (!isDeclared(scopes, as->name)) {
      diags.error(as->nameLoc, "assignment to undeclared identifier '" + as->name + "'");
    }
    return;
  }

  if (auto* ret = dynamic_cast<const ReturnStmt*>(&s)) {
    checkExprImpl(diags, scopes, fns, *ret->valueExpr);
    return;
  }

  if (auto* es = dynamic_cast<const ExprStmt*>(&s)) {
    checkExprImpl(diags, scopes, fns, *es->expr);
    return;
  }

  if (dynamic_cast<const EmptyStmt*>(&s)) {
    return;
  }
}

static void checkExprImpl(Diagnostics& diags, ScopeStack& scopes, const FnSigMap& fns, const Expr& e) {
  if (dynamic_cast<const IntLiteralExpr*>(&e)) return;

  if (auto* vr = dynamic_cast<const VarRefExpr*>(&e)) {
    if (!isDeclared(scopes, vr->name)) {
      diags.error(vr->loc, "use of undeclared identifier '" + vr->name + "'");
    }
    return;
  }

  if (auto* call = dynamic_cast<const CallExpr*>(&e)) {
    auto it = fns.find(call->callee);
    if (it == fns.end()) {
      diags.error(call->calleeLoc, "call to undeclared function '" + call->callee + "'");
      // still check args for more errors
      for (const auto& a : call->args) checkExprImpl(diags, scopes, fns, *a);
      return;
    }

    size_t expected = it->second;
    size_t have = call->args.size();
    if (expected != have) {
      diags.error(call->calleeLoc,
                  "expected " + std::to_string(expected) +
                      " arguments, have " + std::to_string(have));
      // continue to check args
    }

    for (const auto& a : call->args) checkExprImpl(diags, scopes, fns, *a);
    return;
  }

  if (auto* asn = dynamic_cast<const AssignExpr*>(&e)) {
    checkExprImpl(diags, scopes, fns, *asn->rhs);
    if (!isDeclared(scopes, asn->name)) {
      diags.error(asn->nameLoc, "assignment to undeclared identifier '" + asn->name + "'");
    }
    return;
  }

  if (auto* un = dynamic_cast<const UnaryExpr*>(&e)) {
    checkExprImpl(diags, scopes, fns, *un->operand);
    return;
  }

  if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
    checkExprImpl(diags, scopes, fns, *bin->lhs);
    checkExprImpl(diags, scopes, fns, *bin->rhs);
    return;
  }

  // If future Expr kinds exist, silently ignore for now (project minimalism).
}

} // namespace

bool Sema::run(const AstTranslationUnit& tu) {
  // 1) collect function signatures + detect duplicates
  FnSigMap fns;
  {
    std::unordered_set<std::string> seen;
    for (const auto& fn : tu.functions) {
      if (seen.count(fn.name)) {
        diags_.error(fn.nameLoc, "redefinition of '" + fn.name + "'");
        continue;
      }
      seen.insert(fn.name);
      fns[fn.name] = fn.params.size();
    }
  }

  // 2) check each function
  for (const auto& fn : tu.functions) {
    ScopeStack scopes;
    scopes.push_back({}); // function scope

    // parameters as locals
    for (const auto& p : fn.params) {
      auto& cur = scopes.back();
      if (cur.count(p.name)) {
        diags_.error(p.nameLoc, "redefinition of '" + p.name + "'");
        continue;
      }
      cur.insert(p.name);
    }

    for (const auto& st : fn.body) checkStmtImpl(diags_, scopes, fns, /*loopDepth=*/0, *st);
  }

  return !diags_.hasError();
}

} // namespace c99cc
