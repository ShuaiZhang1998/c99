#include "codegen.h"

#include <unordered_map>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"

namespace c99cc {

struct CGEnv {
  llvm::LLVMContext& ctx;
  llvm::IRBuilder<>& b;
  llvm::Function* fn = nullptr;

  std::unordered_map<std::string, llvm::AllocaInst*> locals;
};

static llvm::AllocaInst* createEntryAlloca(CGEnv& env, const std::string& name) {
  llvm::IRBuilder<> tmp(&env.fn->getEntryBlock(), env.fn->getEntryBlock().begin());
  auto* i32 = llvm::Type::getInt32Ty(env.ctx);
  return tmp.CreateAlloca(i32, nullptr, name);
}

static llvm::Value* emitExpr(CGEnv& env, const Expr& e);

static llvm::Value* emitVarRef(CGEnv& env, const VarRefExpr& v) {
  auto it = env.locals.find(v.name);
  if (it == env.locals.end()) {
    return llvm::UndefValue::get(llvm::Type::getInt32Ty(env.ctx));
  }
  return env.b.CreateLoad(it->second->getAllocatedType(), it->second, v.name + ".val");
}

static llvm::Value* emitUnary(CGEnv& env, const UnaryExpr& u) {
  auto* i32 = llvm::Type::getInt32Ty(env.ctx);
  llvm::Value* x = emitExpr(env, *u.operand);

  switch (u.op) {
    case TokenKind::Plus:
      return x;
    case TokenKind::Minus:
      return env.b.CreateNeg(x, "negtmp");
    case TokenKind::Tilde:
      return env.b.CreateNot(x, "nottmp");
    case TokenKind::Bang: {
      llvm::Value* zero = llvm::ConstantInt::get(i32, 0, true);
      llvm::Value* cmp = env.b.CreateICmpEQ(x, zero, "cmptmp"); // i1
      return env.b.CreateZExt(cmp, i32, "booltmp");             // i32
    }
    default:
      break;
  }

  return llvm::UndefValue::get(i32);
}

static llvm::Value* emitExpr(CGEnv& env, const Expr& e) {
  auto* i32 = llvm::Type::getInt32Ty(env.ctx);

  if (auto* lit = dynamic_cast<const IntLiteralExpr*>(&e)) {
    return llvm::ConstantInt::get(i32, (int32_t)lit->value, /*isSigned=*/true);
  }

  if (auto* vr = dynamic_cast<const VarRefExpr*>(&e)) {
    return emitVarRef(env, *vr);
  }

  if (auto* un = dynamic_cast<const UnaryExpr*>(&e)) {
    return emitUnary(env, *un);
  }

  if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
    llvm::Value* L = emitExpr(env, *bin->lhs);
    llvm::Value* R = emitExpr(env, *bin->rhs);

    switch (bin->op) {
      case TokenKind::Plus:  return env.b.CreateAdd(L, R, "addtmp");
      case TokenKind::Minus: return env.b.CreateSub(L, R, "subtmp");
      case TokenKind::Star:  return env.b.CreateMul(L, R, "multmp");
      case TokenKind::Slash: return env.b.CreateSDiv(L, R, "divtmp");

      case TokenKind::Less:
      case TokenKind::Greater:
      case TokenKind::LessEqual:
      case TokenKind::GreaterEqual:
      case TokenKind::EqualEqual:
      case TokenKind::BangEqual: {
        llvm::Value* cmp = nullptr;
        switch (bin->op) {
          case TokenKind::Less:         cmp = env.b.CreateICmpSLT(L, R, "cmptmp"); break;
          case TokenKind::Greater:      cmp = env.b.CreateICmpSGT(L, R, "cmptmp"); break;
          case TokenKind::LessEqual:    cmp = env.b.CreateICmpSLE(L, R, "cmptmp"); break;
          case TokenKind::GreaterEqual: cmp = env.b.CreateICmpSGE(L, R, "cmptmp"); break;
          case TokenKind::EqualEqual:   cmp = env.b.CreateICmpEQ(L, R, "cmptmp");  break;
          case TokenKind::BangEqual:    cmp = env.b.CreateICmpNE(L, R, "cmptmp");  break;
          default: break;
        }
        return env.b.CreateZExt(cmp, i32, "booltmp");
      }

      default:
        break;
    }
  }

  return llvm::UndefValue::get(i32);
}

// Return true if current basic block is terminated (e.g., by return)
static bool emitStmt(CGEnv& env, const Stmt& s);

static bool emitBlock(CGEnv& env, const BlockStmt& blk) {
  for (const auto& st : blk.stmts) {
    if (emitStmt(env, *st)) return true;
  }
  return false;
}

static bool emitIf(CGEnv& env, const IfStmt& iff) {
  auto* i32 = llvm::Type::getInt32Ty(env.ctx);
  llvm::Value* condV = emitExpr(env, *iff.cond);

  // i32 -> i1: cond != 0
  llvm::Value* zero = llvm::ConstantInt::get(i32, 0, true);
  llvm::Value* condI1 = env.b.CreateICmpNE(condV, zero, "ifcond");

  llvm::Function* F = env.fn;
  llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(env.ctx, "if.then", F);
  llvm::BasicBlock* elseBB = nullptr;
  llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(env.ctx, "if.end");

  const bool hasElse = (iff.elseBranch != nullptr);
  if (hasElse) elseBB = llvm::BasicBlock::Create(env.ctx, "if.else", F);

  if (hasElse) env.b.CreateCondBr(condI1, thenBB, elseBB);
  else env.b.CreateCondBr(condI1, thenBB, mergeBB);

  env.b.SetInsertPoint(thenBB);
  bool thenTerminated = emitStmt(env, *iff.thenBranch);
  if (!thenTerminated) env.b.CreateBr(mergeBB);

  bool elseTerminated = false;
  if (hasElse) {
    env.b.SetInsertPoint(elseBB);
    elseTerminated = emitStmt(env, *iff.elseBranch);
    if (!elseTerminated) env.b.CreateBr(mergeBB);
  }

  // Continue at merge
  F->getBasicBlockList().push_back(mergeBB);
  env.b.SetInsertPoint(mergeBB);
  return false;
}

static bool emitWhile(CGEnv& env, const WhileStmt& w) {
  auto* i32 = llvm::Type::getInt32Ty(env.ctx);
  llvm::Function* F = env.fn;

  // Create blocks
  llvm::BasicBlock* condBB = llvm::BasicBlock::Create(env.ctx, "while.cond", F);
  llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(env.ctx, "while.body", F);
  llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(env.ctx, "while.end");

  // Jump from current block to cond
  env.b.CreateBr(condBB);

  // Emit condition
  env.b.SetInsertPoint(condBB);
  llvm::Value* condV = emitExpr(env, *w.cond);
  llvm::Value* zero = llvm::ConstantInt::get(i32, 0, true);
  llvm::Value* condI1 = env.b.CreateICmpNE(condV, zero, "whilecond");
  env.b.CreateCondBr(condI1, bodyBB, endBB);

  // Emit body
  env.b.SetInsertPoint(bodyBB);
  bool bodyTerminated = emitStmt(env, *w.body);
  if (!bodyTerminated) {
    env.b.CreateBr(condBB); // backedge
  }

  // Continue at end
  F->getBasicBlockList().push_back(endBB);
  env.b.SetInsertPoint(endBB);
  return false;
}

static bool emitStmt(CGEnv& env, const Stmt& s) {
  if (auto* d = dynamic_cast<const DeclStmt*>(&s)) {
    auto* slot = createEntryAlloca(env, d->name);
    env.locals[d->name] = slot;

    if (d->initExpr) {
      llvm::Value* initV = emitExpr(env, *d->initExpr);
      env.b.CreateStore(initV, slot);
    }
    return false;
  }

  if (auto* a = dynamic_cast<const AssignStmt*>(&s)) {
    auto it = env.locals.find(a->name);
    if (it == env.locals.end()) return false;
    llvm::Value* v = emitExpr(env, *a->valueExpr);
    env.b.CreateStore(v, it->second);
    return false;
  }

  if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
    llvm::Value* v = emitExpr(env, *r->valueExpr);
    env.b.CreateRet(v);
    return true;
  }

  if (auto* blk = dynamic_cast<const BlockStmt*>(&s)) {
    return emitBlock(env, *blk);
  }

  if (auto* iff = dynamic_cast<const IfStmt*>(&s)) {
    return emitIf(env, *iff);
  }

  if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
    return emitWhile(env, *w);
  }

  return false;
}

std::unique_ptr<llvm::Module> CodeGen::emitLLVM(
    llvm::LLVMContext& ctx,
    const AstTranslationUnit& tu,
    const std::string& moduleName) {

  auto mod = std::make_unique<llvm::Module>(moduleName, ctx);
  llvm::IRBuilder<> b(ctx);

  auto* i32 = llvm::Type::getInt32Ty(ctx);
  auto* fnTy = llvm::FunctionType::get(i32, /*isVarArg=*/false);
  auto* fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, tu.funcName, mod.get());

  auto* entry = llvm::BasicBlock::Create(ctx, "entry", fn);
  b.SetInsertPoint(entry);

  CGEnv env{ctx, b, fn, {}};

  bool terminated = false;
  for (const auto& st : tu.body) {
    terminated = emitStmt(env, *st);
    if (terminated) break;
  }

  if (!terminated) {
    b.CreateRet(llvm::ConstantInt::get(i32, 0, true));
  }

  llvm::verifyFunction(*fn);
  return mod;
}

} // namespace c99cc
