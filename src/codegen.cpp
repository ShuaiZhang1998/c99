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
      return x; // +x
    case TokenKind::Minus:
      return env.b.CreateNeg(x, "negtmp"); // -x
    case TokenKind::Tilde:
      return env.b.CreateNot(x, "nottmp"); // ~x
    case TokenKind::Bang: {
      // !x  => (x == 0) ? 1 : 0
      llvm::Value* zero = llvm::ConstantInt::get(i32, 0, true);
      llvm::Value* cmp = env.b.CreateICmpEQ(x, zero, "cmptmp");   // i1
      return env.b.CreateZExt(cmp, i32, "booltmp");               // i32
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
      default: break;
    }
  }

  return llvm::UndefValue::get(i32);
}

static void emitStmt(CGEnv& env, const Stmt& s) {
  if (auto* d = dynamic_cast<const DeclStmt*>(&s)) {
    auto* slot = createEntryAlloca(env, d->name);
    env.locals[d->name] = slot;

    if (d->initExpr) {
      llvm::Value* initV = emitExpr(env, *d->initExpr);
      env.b.CreateStore(initV, slot);
    }
    return;
  }

  if (auto* a = dynamic_cast<const AssignStmt*>(&s)) {
    auto it = env.locals.find(a->name);
    if (it == env.locals.end()) return;
    llvm::Value* v = emitExpr(env, *a->valueExpr);
    env.b.CreateStore(v, it->second);
    return;
  }

  if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
    llvm::Value* v = emitExpr(env, *r->valueExpr);
    env.b.CreateRet(v);
    return;
  }
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

  bool hasReturn = false;
  for (const auto& st : tu.body) {
    emitStmt(env, *st);
    if (dynamic_cast<ReturnStmt*>(st.get()) != nullptr) {
      hasReturn = true;
      break;
    }
  }

  if (!hasReturn) {
    b.CreateRet(llvm::ConstantInt::get(i32, 0, true));
  }

  llvm::verifyFunction(*fn);
  return mod;
}

} // namespace c99cc
