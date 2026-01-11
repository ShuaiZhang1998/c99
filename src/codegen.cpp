#include "codegen.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Verifier.h>

namespace c99cc {

namespace {

struct CGEnv {
  llvm::LLVMContext& ctx;
  llvm::Module& mod;
  llvm::IRBuilder<>& b;

  llvm::Function* fn = nullptr;

  // function table: name -> llvm::Function*
  std::unordered_map<std::string, llvm::Function*> functions;

  // local scopes: name -> alloca
  std::vector<std::unordered_map<std::string, llvm::AllocaInst*>> scopes;

  // loop stack: {breakTarget, continueTarget}
  std::vector<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> loops;

  llvm::Type* i32Ty() { return llvm::Type::getInt32Ty(ctx); }
  llvm::Type* i1Ty() { return llvm::Type::getInt1Ty(ctx); }

  void pushScope() { scopes.emplace_back(); }
  void popScope() { scopes.pop_back(); }

  void resetFunctionState(llvm::Function* f) {
    fn = f;
    scopes.clear();
    loops.clear();
  }

  llvm::AllocaInst* lookup(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto f = it->find(name);
      if (f != it->end()) return f->second;
    }
    return nullptr;
  }

  bool insertLocal(const std::string& name, llvm::AllocaInst* slot) {
    auto& cur = scopes.back();
    if (cur.count(name)) return false;
    cur[name] = slot;
    return true;
  }
};

static llvm::AllocaInst* createEntryAlloca(CGEnv& env, const std::string& name) {
  llvm::IRBuilder<> tmp(&env.fn->getEntryBlock(), env.fn->getEntryBlock().begin());
  return tmp.CreateAlloca(env.i32Ty(), nullptr, name);
}

static llvm::Value* i32Const(CGEnv& env, int64_t v) {
  return llvm::ConstantInt::get(env.i32Ty(), (uint64_t)v, true);
}

static llvm::Value* asBoolI1(CGEnv& env, llvm::Value* v) {
  if (v->getType()->isIntegerTy(1)) return v;
  return env.b.CreateICmpNE(v, i32Const(env, 0), "tobool");
}

// forward decl
static llvm::Value* emitExpr(CGEnv& env, const Expr& e);
static bool emitStmt(CGEnv& env, const Stmt& s);

// -------------------- Expr --------------------

static llvm::Value* emitUnary(CGEnv& env, const UnaryExpr& u) {
  llvm::Value* opnd = emitExpr(env, *u.operand);
  switch (u.op) {
    case TokenKind::Plus:
      return opnd;
    case TokenKind::Minus:
      return env.b.CreateSub(i32Const(env, 0), opnd, "neg");
    case TokenKind::Tilde:
      return env.b.CreateNot(opnd, "bitnot");
    case TokenKind::Bang: {
      llvm::Value* b = asBoolI1(env, opnd);
      llvm::Value* inv = env.b.CreateNot(b, "lnot");
      return env.b.CreateZExt(inv, env.i32Ty(), "lnot.i32");
    }
    default:
      return i32Const(env, 0);
  }
}

static llvm::Value* emitShortCircuitAnd(CGEnv& env, const BinaryExpr& bin) {
  llvm::Function* F = env.fn;

  llvm::Value* lhsV = emitExpr(env, *bin.lhs);
  llvm::Value* lhsB = asBoolI1(env, lhsV);

  llvm::BasicBlock* curBB = env.b.GetInsertBlock();
  llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(env.ctx, "land.rhs", F);
  llvm::BasicBlock* endBB = llvm::BasicBlock::Create(env.ctx, "land.end", F);

  env.b.CreateCondBr(lhsB, rhsBB, endBB);

  env.b.SetInsertPoint(rhsBB);
  llvm::Value* rhsV = emitExpr(env, *bin.rhs);
  llvm::Value* rhsB = asBoolI1(env, rhsV);
  env.b.CreateBr(endBB);

  env.b.SetInsertPoint(endBB);
  llvm::PHINode* phi = env.b.CreatePHI(env.i1Ty(), 2, "land.phi");
  phi->addIncoming(llvm::ConstantInt::getFalse(env.ctx), curBB);
  phi->addIncoming(rhsB, rhsBB);

  return env.b.CreateZExt(phi, env.i32Ty(), "land.i32");
}

static llvm::Value* emitShortCircuitOr(CGEnv& env, const BinaryExpr& bin) {
  llvm::Function* F = env.fn;

  llvm::Value* lhsV = emitExpr(env, *bin.lhs);
  llvm::Value* lhsB = asBoolI1(env, lhsV);

  llvm::BasicBlock* curBB = env.b.GetInsertBlock();
  llvm::BasicBlock* rhsBB = llvm::BasicBlock::Create(env.ctx, "lor.rhs", F);
  llvm::BasicBlock* endBB = llvm::BasicBlock::Create(env.ctx, "lor.end", F);

  env.b.CreateCondBr(lhsB, endBB, rhsBB);

  env.b.SetInsertPoint(rhsBB);
  llvm::Value* rhsV = emitExpr(env, *bin.rhs);
  llvm::Value* rhsB = asBoolI1(env, rhsV);
  env.b.CreateBr(endBB);

  env.b.SetInsertPoint(endBB);
  llvm::PHINode* phi = env.b.CreatePHI(env.i1Ty(), 2, "lor.phi");
  phi->addIncoming(llvm::ConstantInt::getTrue(env.ctx), curBB);
  phi->addIncoming(rhsB, rhsBB);

  return env.b.CreateZExt(phi, env.i32Ty(), "lor.i32");
}

static llvm::Value* emitBinary(CGEnv& env, const BinaryExpr& bin) {
  if (bin.op == TokenKind::AmpAmp) return emitShortCircuitAnd(env, bin);
  if (bin.op == TokenKind::PipePipe) return emitShortCircuitOr(env, bin);

  llvm::Value* L = emitExpr(env, *bin.lhs);
  llvm::Value* R = emitExpr(env, *bin.rhs);

  switch (bin.op) {
    case TokenKind::Plus:  return env.b.CreateAdd(L, R, "add");
    case TokenKind::Minus: return env.b.CreateSub(L, R, "sub");
    case TokenKind::Star:  return env.b.CreateMul(L, R, "mul");
    case TokenKind::Slash: return env.b.CreateSDiv(L, R, "div");

    case TokenKind::Comma:
      // value of comma expr is RHS (LHS evaluated for side effects already)
      return R;

    case TokenKind::Less: {
      auto* c = env.b.CreateICmpSLT(L, R, "cmp");
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }
    case TokenKind::LessEqual: {
      auto* c = env.b.CreateICmpSLE(L, R, "cmp");
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }
    case TokenKind::Greater: {
      auto* c = env.b.CreateICmpSGT(L, R, "cmp");
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }
    case TokenKind::GreaterEqual: {
      auto* c = env.b.CreateICmpSGE(L, R, "cmp");
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }
    case TokenKind::EqualEqual: {
      auto* c = env.b.CreateICmpEQ(L, R, "cmp");
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }
    case TokenKind::BangEqual: {
      auto* c = env.b.CreateICmpNE(L, R, "cmp");
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }

    default:
      return i32Const(env, 0);
  }
}

static llvm::Value* emitExpr(CGEnv& env, const Expr& e) {
  if (auto* lit = dynamic_cast<const IntLiteralExpr*>(&e)) {
    return i32Const(env, lit->value);
  }

  if (auto* vr = dynamic_cast<const VarRefExpr*>(&e)) {
    llvm::AllocaInst* slot = env.lookup(vr->name);
    if (!slot) return i32Const(env, 0);
    return env.b.CreateLoad(env.i32Ty(), slot, vr->name + ".val");
  }

  if (auto* call = dynamic_cast<const CallExpr*>(&e)) {
    llvm::Function* callee = nullptr;
    auto it = env.functions.find(call->callee);
    if (it != env.functions.end()) callee = it->second;
    if (!callee) return i32Const(env, 0);

    std::vector<llvm::Value*> argsV;
    argsV.reserve(call->args.size());
    for (const auto& a : call->args) argsV.push_back(emitExpr(env, *a));

    return env.b.CreateCall(callee, argsV, "calltmp");
  }

  if (auto* un = dynamic_cast<const UnaryExpr*>(&e)) {
    return emitUnary(env, *un);
  }

  if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
    return emitBinary(env, *bin);
  }

  if (auto* asn = dynamic_cast<const AssignExpr*>(&e)) {
    llvm::AllocaInst* slot = env.lookup(asn->name);
    llvm::Value* rhsV = emitExpr(env, *asn->rhs);
    if (slot) env.b.CreateStore(rhsV, slot);
    return rhsV;
  }

  return i32Const(env, 0);
}

// -------------------- Stmt --------------------

static bool emitBlock(CGEnv& env, const BlockStmt& blk) {
  env.pushScope();
  bool terminated = false;
  for (const auto& st : blk.stmts) {
    terminated = emitStmt(env, *st);
    if (terminated) break;
  }
  env.popScope();
  return terminated;
}

static bool emitIf(CGEnv& env, const IfStmt& s) {
  llvm::Function* F = env.fn;

  llvm::Value* condV = emitExpr(env, *s.cond);
  llvm::Value* condB = asBoolI1(env, condV);

  llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(env.ctx, "if.then", F);
  llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(env.ctx, "if.else");
  llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(env.ctx, "if.end");

  if (s.elseBranch) {
    F->getBasicBlockList().push_back(elseBB);
    env.b.CreateCondBr(condB, thenBB, elseBB);
  } else {
    env.b.CreateCondBr(condB, thenBB, endBB);
  }

  env.b.SetInsertPoint(thenBB);
  bool thenTerm = emitStmt(env, *s.thenBranch);
  if (!thenTerm && !env.b.GetInsertBlock()->getTerminator()) env.b.CreateBr(endBB);

  if (s.elseBranch) {
    F->getBasicBlockList().push_back(elseBB);
    env.b.SetInsertPoint(elseBB);
    bool elseTerm = emitStmt(env, *s.elseBranch);
    if (!elseTerm && !env.b.GetInsertBlock()->getTerminator()) env.b.CreateBr(endBB);
  }

  F->getBasicBlockList().push_back(endBB);
  env.b.SetInsertPoint(endBB);
  return false;
}

static bool emitWhile(CGEnv& env, const WhileStmt& s) {
  llvm::Function* F = env.fn;

  llvm::BasicBlock* condBB = llvm::BasicBlock::Create(env.ctx, "while.cond", F);
  llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(env.ctx, "while.body", F);
  llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(env.ctx, "while.end");

  env.b.CreateBr(condBB);

  env.b.SetInsertPoint(condBB);
  llvm::Value* condV = emitExpr(env, *s.cond);
  llvm::Value* condB = asBoolI1(env, condV);
  env.b.CreateCondBr(condB, bodyBB, endBB);

  env.b.SetInsertPoint(bodyBB);
  env.loops.push_back({endBB, condBB}); // continue => cond
  bool bodyTerm = emitStmt(env, *s.body);
  env.loops.pop_back();
  if (!bodyTerm && !env.b.GetInsertBlock()->getTerminator()) env.b.CreateBr(condBB);

  F->getBasicBlockList().push_back(endBB);
  env.b.SetInsertPoint(endBB);
  return false;
}

static bool emitDoWhile(CGEnv& env, const DoWhileStmt& s) {
  llvm::Function* F = env.fn;

  llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(env.ctx, "do.body", F);
  llvm::BasicBlock* condBB = llvm::BasicBlock::Create(env.ctx, "do.cond", F);
  llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(env.ctx, "do.end");

  env.b.CreateBr(bodyBB);

  env.b.SetInsertPoint(bodyBB);
  env.loops.push_back({endBB, condBB}); // continue => cond
  bool bodyTerm = emitStmt(env, *s.body);
  env.loops.pop_back();
  if (!bodyTerm && !env.b.GetInsertBlock()->getTerminator()) env.b.CreateBr(condBB);

  env.b.SetInsertPoint(condBB);
  llvm::Value* condV = emitExpr(env, *s.cond);
  llvm::Value* condB = asBoolI1(env, condV);
  env.b.CreateCondBr(condB, bodyBB, endBB);

  F->getBasicBlockList().push_back(endBB);
  env.b.SetInsertPoint(endBB);
  return false;
}

static bool emitFor(CGEnv& env, const ForStmt& s) {
  llvm::Function* F = env.fn;

  env.pushScope(); // for-scope

  if (s.init) {
    bool t = emitStmt(env, *s.init);
    if (t) { env.popScope(); return true; }
  }

  llvm::BasicBlock* condBB = llvm::BasicBlock::Create(env.ctx, "for.cond", F);
  llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(env.ctx, "for.body", F);
  llvm::BasicBlock* incBB  = llvm::BasicBlock::Create(env.ctx, "for.inc", F);
  llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(env.ctx, "for.end");

  env.b.CreateBr(condBB);

  env.b.SetInsertPoint(condBB);
  llvm::Value* condB = s.cond ? asBoolI1(env, emitExpr(env, *s.cond))
                              : llvm::ConstantInt::getTrue(env.ctx);
  env.b.CreateCondBr(condB, bodyBB, endBB);

  env.b.SetInsertPoint(bodyBB);
  env.loops.push_back({endBB, incBB}); // continue => inc
  bool bodyTerm = emitStmt(env, *s.body);
  env.loops.pop_back();
  if (!bodyTerm && !env.b.GetInsertBlock()->getTerminator()) env.b.CreateBr(incBB);

  env.b.SetInsertPoint(incBB);
  if (s.inc) (void)emitExpr(env, *s.inc);
  if (!env.b.GetInsertBlock()->getTerminator()) env.b.CreateBr(condBB);

  F->getBasicBlockList().push_back(endBB);
  env.b.SetInsertPoint(endBB);

  env.popScope();
  return false;
}

static bool emitStmt(CGEnv& env, const Stmt& s) {
  if (env.b.GetInsertBlock()->getTerminator()) return true;

  if (auto* blk = dynamic_cast<const BlockStmt*>(&s)) return emitBlock(env, *blk);

  if (auto* d = dynamic_cast<const DeclStmt*>(&s)) {
    llvm::AllocaInst* slot = createEntryAlloca(env, d->name);
    env.insertLocal(d->name, slot);
    if (d->initExpr) env.b.CreateStore(emitExpr(env, *d->initExpr), slot);
    else env.b.CreateStore(i32Const(env, 0), slot);
    return false;
  }

  // legacy AssignStmt (if still produced somewhere)
  if (auto* a = dynamic_cast<const AssignStmt*>(&s)) {
    llvm::AllocaInst* slot = env.lookup(a->name);
    llvm::Value* rhsV = emitExpr(env, *a->valueExpr);
    if (slot) env.b.CreateStore(rhsV, slot);
    return false;
  }

  if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
    env.b.CreateRet(emitExpr(env, *r->valueExpr));
    return true;
  }

  if (auto* br = dynamic_cast<const BreakStmt*>(&s)) {
    if (!env.loops.empty()) {
      env.b.CreateBr(env.loops.back().first);
      return true;
    }
    return false;
  }

  if (auto* co = dynamic_cast<const ContinueStmt*>(&s)) {
    if (!env.loops.empty()) {
      env.b.CreateBr(env.loops.back().second);
      return true;
    }
    return false;
  }

  if (auto* iff = dynamic_cast<const IfStmt*>(&s)) return emitIf(env, *iff);
  if (auto* wh  = dynamic_cast<const WhileStmt*>(&s)) return emitWhile(env, *wh);
  if (auto* dw  = dynamic_cast<const DoWhileStmt*>(&s)) return emitDoWhile(env, *dw);
  if (auto* fo  = dynamic_cast<const ForStmt*>(&s)) return emitFor(env, *fo);

  if (auto* es = dynamic_cast<const ExprStmt*>(&s)) {
    (void)emitExpr(env, *es->expr);
    return false;
  }

  if (dynamic_cast<const EmptyStmt*>(&s)) {
    return false;
  }

  return false;
}

} // namespace

// -------------------- CodeGen entry --------------------

std::unique_ptr<llvm::Module> CodeGen::emitLLVM(
    llvm::LLVMContext& ctx,
    const AstTranslationUnit& tu,
    const std::string& moduleName) {
  auto mod = std::make_unique<llvm::Module>(moduleName, ctx);
  llvm::IRBuilder<> builder(ctx);
  CGEnv env{ctx, *mod, builder};

  // 1) predeclare all functions
  for (const auto& fn : tu.functions) {
    std::vector<llvm::Type*> paramTys(fn.params.size(), llvm::Type::getInt32Ty(ctx));
    auto* fnTy = llvm::FunctionType::get(llvm::Type::getInt32Ty(ctx), paramTys, false);
    llvm::Function* F = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, fn.name, mod.get());
    env.functions[fn.name] = F;

    // name LLVM args for readability
    unsigned i = 0;
    for (auto& arg : F->args()) {
      if (i < fn.params.size()) arg.setName(fn.params[i].name);
      ++i;
    }
  }

  // 2) emit each function body
  for (const auto& fnAst : tu.functions) {
    llvm::Function* F = env.functions[fnAst.name];
    assert(F && "function must have been declared");

    // fresh function body (in case module reused)
    // (LLVM allows reusing F but not here; ensure it's empty)
    if (!F->empty()) {
      // If already has body, skip to avoid verifier issues in dev workflow.
      continue;
    }

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", F);
    builder.SetInsertPoint(entry);

    env.resetFunctionState(F);
    env.pushScope(); // function scope

    // lower parameters: alloca in entry + store
    unsigned idx = 0;
    for (auto& arg : F->args()) {
      std::string pname = (idx < fnAst.params.size()) ? fnAst.params[idx].name : std::string("arg");
      llvm::AllocaInst* slot = createEntryAlloca(env, pname);
      env.insertLocal(pname, slot);
      builder.CreateStore(&arg, slot);
      ++idx;
    }

    bool terminated = false;
    for (const auto& st : fnAst.body) {
      terminated = emitStmt(env, *st);
      if (terminated) break;
    }

    if (!builder.GetInsertBlock()->getTerminator()) {
      builder.CreateRet(i32Const(env, 0));
    }

    env.popScope();
    llvm::verifyFunction(*F);
  }

  return mod;
}

} // namespace c99cc
