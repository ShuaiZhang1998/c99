#include "sema.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace c99cc {

namespace {

using Scope = std::unordered_set<std::string>;
using ScopeStack = std::vector<Scope>;

static bool isDeclaredVar(const ScopeStack& scopes, const std::string& name) {
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    if (it->count(name)) return true;
  }
  return false;
}

// ---- function table ----

struct FnInfo {
  size_t arity = 0;
  bool hasDecl = false;
  bool hasDef = false;
  SourceLocation firstLoc{};
};

using FnTable = std::unordered_map<std::string, FnInfo>;

static bool sameSignature(const FnInfo& info, size_t arity) {
  return info.arity == arity;
}

// ---- expr/stmt checking ----

static void checkExprImpl(Diagnostics& diags, ScopeStack& scopes, const FnTable& fns, const Expr& e);

static void checkStmtImpl(
    Diagnostics& diags,
    ScopeStack& scopes,
    const FnTable& fns,
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
    // for introduces its own scope (matches your existing tests)
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
    auto& cur = scopes.back();
    if (cur.count(decl->name)) {
      diags.error(decl->nameLoc, "redefinition of '" + decl->name + "'");
      return;
    }

    // initializer cannot reference the variable being declared:
    // keep behavior by checking before insertion.
    if (decl->initExpr) checkExprImpl(diags, scopes, fns, *decl->initExpr);

    cur.insert(decl->name);
    return;
  }

  if (auto* as = dynamic_cast<const AssignStmt*>(&s)) {
    // legacy stmt (if still exists somewhere)
    checkExprImpl(diags, scopes, fns, *as->valueExpr);
    if (!isDeclaredVar(scopes, as->name)) {
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

  if (dynamic_cast<const EmptyStmt*>(&s)) return;
}

static void checkExprImpl(Diagnostics& diags, ScopeStack& scopes, const FnTable& fns, const Expr& e) {
  if (dynamic_cast<const IntLiteralExpr*>(&e)) return;

  if (auto* vr = dynamic_cast<const VarRefExpr*>(&e)) {
    if (!isDeclaredVar(scopes, vr->name)) {
      diags.error(vr->loc, "use of undeclared identifier '" + vr->name + "'");
    }
    return;
  }

  if (auto* call = dynamic_cast<const CallExpr*>(&e)) {
    auto it = fns.find(call->callee);
    if (it == fns.end()) {
      diags.error(call->calleeLoc, "call to undeclared function '" + call->callee + "'");
      for (const auto& a : call->args) checkExprImpl(diags, scopes, fns, *a);
      return;
    }

    size_t expected = it->second.arity;
    size_t have = call->args.size();
    if (expected != have) {
      diags.error(call->calleeLoc,
                  "expected " + std::to_string(expected) +
                      " arguments, have " + std::to_string(have));
    }

    for (const auto& a : call->args) checkExprImpl(diags, scopes, fns, *a);
    return;
  }

  if (auto* asn = dynamic_cast<const AssignExpr*>(&e)) {
    checkExprImpl(diags, scopes, fns, *asn->rhs);
    if (!isDeclaredVar(scopes, asn->name)) {
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
}

static size_t protoArity(const FunctionProto& p) {
  return p.params.size();
}

static void addOrCheckFn(
    Diagnostics& diags,
    FnTable& fns,
    const std::string& name,
    SourceLocation nameLoc,
    size_t arity,
    bool isDef) {
  auto it = fns.find(name);
  if (it == fns.end()) {
    FnInfo info;
    info.arity = arity;
    info.firstLoc = nameLoc;
    info.hasDecl = !isDef;
    info.hasDef = isDef;
    fns.emplace(name, info);
    return;
  }

  FnInfo& info = it->second;

  // signature mismatch
  if (!sameSignature(info, arity)) {
    diags.error(nameLoc,
                "conflicting types for '" + name +
                    "' (previous declaration has different parameter count)");
    return;
  }

  // same signature: update decl/def flags
  if (isDef) {
    if (info.hasDef) {
      diags.error(nameLoc, "redefinition of '" + name + "'");
      return;
    }
    info.hasDef = true;
  } else {
    info.hasDecl = true; // repeated decl ok
  }
}

} // namespace

bool Sema::run(const AstTranslationUnit& tu) {
  // 1) collect all function prototypes (decls + defs)
  FnTable fns;
  for (const auto& item : tu.items) {
    if (auto* d = std::get_if<FunctionDecl>(&item)) {
      const auto& p = d->proto;
      addOrCheckFn(diags_, fns, p.name, p.nameLoc, protoArity(p), /*isDef=*/false);
    } else if (auto* def = std::get_if<FunctionDef>(&item)) {
      const auto& p = def->proto;
      addOrCheckFn(diags_, fns, p.name, p.nameLoc, protoArity(p), /*isDef=*/true);
    }
  }

  // 2) check bodies of function definitions
  for (const auto& item : tu.items) {
    auto* def = std::get_if<FunctionDef>(&item);
    if (!def) continue;

    ScopeStack scopes;
    scopes.push_back({}); // function scope

    // parameters as locals ONLY if they have names
    for (const auto& prm : def->proto.params) {
      if (!prm.name.has_value()) continue;
      const std::string& pname = *prm.name;
      auto& cur = scopes.back();
      if (cur.count(pname)) {
        diags_.error(prm.nameLoc, "redefinition of '" + pname + "'");
        continue;
      }
      cur.insert(pname);
    }

    for (const auto& st : def->body) checkStmtImpl(diags_, scopes, fns, /*loopDepth=*/0, *st);
  }

  return !diags_.hasError();
}

} // namespace c99cc
