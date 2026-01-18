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
  Type currentReturnType{};

  // function table: name -> llvm::Function*
  std::unordered_map<std::string, llvm::Function*> functions;
  std::unordered_map<std::string, std::vector<Type>> functionParamTypes;

  // struct table: name -> llvm::StructType*
  std::unordered_map<std::string, llvm::StructType*> structs;
  std::unordered_map<std::string, std::vector<StructField>> structFields;
  std::unordered_map<std::string, int64_t> enumConstants;

  struct GlobalBinding {
    llvm::GlobalVariable* gv = nullptr;
    Type type;
  };

  struct LocalBinding {
    llvm::AllocaInst* slot = nullptr;
    Type type;
  };

  // global variables: name -> binding
  std::unordered_map<std::string, GlobalBinding> globals;

  // local scopes: name -> binding
  std::vector<std::unordered_map<std::string, LocalBinding>> scopes;

  // loop stack: {breakTarget, continueTarget}
  std::vector<std::pair<llvm::BasicBlock*, llvm::BasicBlock*>> loops;

  llvm::Type* i32Ty() { return llvm::Type::getInt32Ty(ctx); }
  llvm::Type* i1Ty() { return llvm::Type::getInt1Ty(ctx); }

  void pushScope() { scopes.emplace_back(); }
  void popScope() { scopes.pop_back(); }

  void resetFunctionState(llvm::Function* f) {
    fn = f;
    currentReturnType = Type{};
    scopes.clear();
    loops.clear();
  }

  LocalBinding* lookupLocal(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
      auto f = it->find(name);
      if (f != it->end()) return &f->second;
    }
    return nullptr;
  }

  GlobalBinding* lookupGlobal(const std::string& name) {
    auto it = globals.find(name);
    if (it == globals.end()) return nullptr;
    return &it->second;
  }

  bool insertLocal(const std::string& name, llvm::AllocaInst* slot, const Type& type) {
    auto& cur = scopes.back();
    if (cur.count(name)) return false;
    cur.emplace(name, LocalBinding{slot, type});
    return true;
  }

  bool insertGlobal(const std::string& name, llvm::GlobalVariable* gv, const Type& type) {
    if (globals.count(name)) return false;
    globals.emplace(name, GlobalBinding{gv, type});
    return true;
  }
};

static llvm::Type* llvmType(CGEnv& env, const Type& t) {
  llvm::Type* baseTy = nullptr;
  if (t.base == Type::Base::Void) {
    baseTy = llvm::Type::getInt8Ty(env.ctx);
  } else if (t.base == Type::Base::Struct) {
    auto it = env.structs.find(t.structName);
    assert(it != env.structs.end());
    baseTy = it->second;
  } else if (t.base == Type::Base::Enum) {
    baseTy = env.i32Ty();
  } else if (t.base == Type::Base::Char) {
    baseTy = llvm::Type::getInt8Ty(env.ctx);
  } else if (t.base == Type::Base::Short) {
    baseTy = llvm::Type::getInt16Ty(env.ctx);
  } else if (t.base == Type::Base::Long) {
    baseTy = llvm::Type::getInt64Ty(env.ctx);
  } else if (t.base == Type::Base::LongLong) {
    baseTy = llvm::Type::getInt64Ty(env.ctx);
  } else if (t.base == Type::Base::Float) {
    baseTy = llvm::Type::getFloatTy(env.ctx);
  } else if (t.base == Type::Base::Double) {
    baseTy = llvm::Type::getDoubleTy(env.ctx);
  } else {
    baseTy = env.i32Ty();
  }
  llvm::Type* ty = baseTy;
  if (t.ptrOutsideArrays) {
    for (auto it = t.arrayDims.rbegin(); it != t.arrayDims.rend(); ++it) {
      size_t dim = it->value_or(0);
      ty = llvm::ArrayType::get(ty, dim);
    }
    for (int i = 0; i < t.ptrDepth; ++i) {
      ty = llvm::PointerType::getUnqual(ty);
    }
  } else {
    for (int i = 0; i < t.ptrDepth; ++i) {
      ty = llvm::PointerType::getUnqual(ty);
    }
    for (auto it = t.arrayDims.rbegin(); it != t.arrayDims.rend(); ++it) {
      size_t dim = it->value_or(0);
      ty = llvm::ArrayType::get(ty, dim);
    }
  }
  return ty;
}

static llvm::AllocaInst* createEntryAlloca(CGEnv& env, const std::string& name, const Type& type) {
  llvm::IRBuilder<> tmp(&env.fn->getEntryBlock(), env.fn->getEntryBlock().begin());
  return tmp.CreateAlloca(llvmType(env, type), nullptr, name);
}

static llvm::Value* i32Const(CGEnv& env, int64_t v) {
  return llvm::ConstantInt::get(env.i32Ty(), (uint64_t)v, true);
}

static llvm::Value* emitStringLiteral(CGEnv& env, const std::string& value) {
  return env.b.CreateGlobalStringPtr(value, ".str");
}

static llvm::Value* castIntegerToType(CGEnv& env, llvm::Value* v, const Type& src, const Type& dst) {
  llvm::Type* dstTy = llvmType(env, dst);
  if (v->getType() == dstTy) return v;
  if (v->getType()->isIntegerTy() && dstTy->isIntegerTy()) {
    if (src.isUnsigned) return env.b.CreateZExtOrTrunc(v, dstTy, "int.cast");
    return env.b.CreateSExtOrTrunc(v, dstTy, "int.cast");
  }
  return v;
}

static llvm::Value* castNumericToType(
    CGEnv& env, llvm::Value* v, const Type& src, const Type& dst) {
  if (src == dst) return v;
  llvm::Type* dstTy = llvmType(env, dst);
  if (dst.isInteger()) {
    if (src.isInteger()) return castIntegerToType(env, v, src, dst);
    if (src.isFloating()) {
      if (dst.isUnsigned) return env.b.CreateFPToUI(v, dstTy, "fp.to.ui");
      return env.b.CreateFPToSI(v, dstTy, "fp.to.si");
    }
    return v;
  }
  if (dst.isFloating()) {
    if (src.isInteger()) {
      if (src.isUnsigned) return env.b.CreateUIToFP(v, dstTy, "ui.to.fp");
      return env.b.CreateSIToFP(v, dstTy, "si.to.fp");
    }
    if (src.isFloating()) {
      if (v->getType() == dstTy) return v;
      unsigned srcBits = v->getType()->getPrimitiveSizeInBits();
      unsigned dstBits = dstTy->getPrimitiveSizeInBits();
      if (srcBits < dstBits) return env.b.CreateFPExt(v, dstTy, "fp.ext");
      return env.b.CreateFPTrunc(v, dstTy, "fp.trunc");
    }
    return v;
  }
  return v;
}

static llvm::Value* castIndex(CGEnv& env, llvm::Value* v, const Type& idxTy) {
  if (v->getType()->isIntegerTy(64)) return v;
  if (v->getType()->isIntegerTy()) {
    if (idxTy.isUnsigned) return env.b.CreateZExtOrTrunc(v, env.b.getInt64Ty(), "idx.cast");
    return env.b.CreateSExtOrTrunc(v, env.b.getInt64Ty(), "idx.cast");
  }
  return v;
}

static uint64_t integerSizeBytes(const Type& t) {
  switch (t.base) {
    case Type::Base::Char:  return 1;
    case Type::Base::Short: return 2;
    case Type::Base::Long:  return 8;
    case Type::Base::LongLong: return 8;
    case Type::Base::Int:   return 4;
    case Type::Base::Enum:  return 4;
    case Type::Base::Float: return 4;
    case Type::Base::Double:return 8;
    default: return sizeof(void*);
  }
}

static uint64_t sizeOfType(const Type& t, const CGEnv& env) {
  if (t.isPointer() || (t.ptrOutsideArrays && t.ptrDepth > 0)) return sizeof(void*);
  if (t.isArray() && !t.ptrOutsideArrays) {
    uint64_t elemSize = sizeOfType(t.elementType(), env);
    uint64_t total = elemSize;
    for (const auto& dim : t.arrayDims) {
      if (!dim.has_value()) return sizeof(void*);
      total *= *dim;
    }
    return total;
  }
  if (t.base == Type::Base::Struct && t.ptrDepth == 0) {
    auto it = env.structFields.find(t.structName);
    if (it == env.structFields.end()) return sizeof(void*);
    uint64_t total = 0;
    for (const auto& field : it->second) {
      total += sizeOfType(field.type, env);
    }
    return total;
  }
  if (t.isNumeric()) return integerSizeBytes(t);
  return sizeof(void*);
}

static int integerRank(const Type& t) {
  switch (t.base) {
    case Type::Base::Char: return 1;
    case Type::Base::Short: return 2;
    case Type::Base::Int: return 3;
    case Type::Base::Enum: return 3;
    case Type::Base::Long: return 4;
    case Type::Base::LongLong: return 5;
    default: return 0;
  }
}

static Type typeFromRank(int rank) {
  Type t;
  switch (rank) {
    case 1: t.base = Type::Base::Char; break;
    case 2: t.base = Type::Base::Short; break;
    case 4: t.base = Type::Base::Long; break;
    case 5: t.base = Type::Base::LongLong; break;
    case 3:
    default: t.base = Type::Base::Int; break;
  }
  return t;
}

static Type promoteInteger(const Type& t) {
  Type res = t;
  if (!t.isInteger()) return res;
  if (t.base == Type::Base::Enum) {
    res.base = Type::Base::Int;
    res.enumName.clear();
    return res;
  }
  if (t.base == Type::Base::Char || t.base == Type::Base::Short) {
    res.base = Type::Base::Int;
  }
  return res;
}

static Type commonIntegerType(const Type& lhs, const Type& rhs) {
  int rank = std::max(integerRank(lhs), integerRank(rhs));
  Type t = typeFromRank(rank);
  t.isUnsigned = lhs.isUnsigned || rhs.isUnsigned;
  return t;
}

static Type commonNumericType(const Type& lhs, const Type& rhs) {
  if (lhs.isFloating() || rhs.isFloating()) {
    if (lhs.base == Type::Base::Double || rhs.base == Type::Base::Double) {
      return Type{Type::Base::Double, 0};
    }
    return Type{Type::Base::Float, 0};
  }
  return commonIntegerType(lhs, rhs);
}

static llvm::Value* asBoolI1(CGEnv& env, llvm::Value* v) {
  if (v->getType()->isIntegerTy(1)) return v;
  if (v->getType()->isPointerTy()) {
    auto* pty = llvm::cast<llvm::PointerType>(v->getType());
    auto* zero = llvm::ConstantPointerNull::get(pty);
    return env.b.CreateICmpNE(v, zero, "tobool");
  }
  if (v->getType()->isFloatingPointTy()) {
    auto* zero = llvm::ConstantFP::get(v->getType(), 0.0);
    return env.b.CreateFCmpONE(v, zero, "tobool");
  }
  if (v->getType()->isIntegerTy()) {
    auto* zero = llvm::ConstantInt::get(v->getType(), 0, true);
    return env.b.CreateICmpNE(v, zero, "tobool");
  }
  return env.b.CreateICmpNE(v, i32Const(env, 0), "tobool");
}

static const FunctionProto* getProto(const TopLevelItem& item) {
  if (auto* d = std::get_if<FunctionDecl>(&item)) return &d->proto;
  if (auto* f = std::get_if<FunctionDef>(&item)) return &f->proto;
  return nullptr;
}

static const Type& exprType(const Expr& e) {
  assert(e.semaType.has_value());
  return *e.semaType;
}

static bool isNullPointerLiteral(const Expr& e) {
  if (auto* lit = dynamic_cast<const IntLiteralExpr*>(&e)) return lit->value == 0;
  return false;
}

static llvm::Value* castPointerIfNeeded(CGEnv& env, llvm::Value* v, llvm::Type* dstTy) {
  if (v->getType() == dstTy) return v;
  return env.b.CreateBitCast(v, dstTy, "ptr.cast");
}

static llvm::Constant* zeroValue(CGEnv& env, const Type& t) {
  llvm::Type* ty = llvmType(env, t);
  if (t.isPointer()) {
    return llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ty));
  }
  if (t.isArray() || (t.base == Type::Base::Struct && t.ptrDepth == 0)) {
    return llvm::ConstantAggregateZero::get(ty);
  }
  if (t.isInteger()) {
    return llvm::ConstantInt::get(ty, 0, true);
  }
  if (t.isFloating()) {
    return llvm::ConstantFP::get(ty, 0.0);
  }
  return llvm::ConstantInt::get(env.i32Ty(), 0, true);
}

static Type adjustParamType(const Type& t) {
  if (!t.isArray()) return t;
  return t.decayType();
}

static llvm::Value* decayArrayToPointer(CGEnv& env, llvm::Value* addr, const Type& arrayTy) {
  llvm::Value* zero = i32Const(env, 0);
  llvm::Value* idxs[] = {zero, zero};
  llvm::Type* arrTy = llvmType(env, arrayTy);
  return env.b.CreateGEP(arrTy, addr, idxs, "arr.decay");
}

// forward decl
static llvm::Value* emitExpr(CGEnv& env, const Expr& e);

static llvm::Value* emitEqualByAddr(
    CGEnv& env, const Type& ty, llvm::Value* lhsAddr, llvm::Value* rhsAddr) {
  if (ty.isPointer() || ty.isInteger() || ty.isFloating()) {
    llvm::Type* elemTy = llvmType(env, ty);
    llvm::Value* L = env.b.CreateLoad(elemTy, lhsAddr, "cmp.l");
    llvm::Value* R = env.b.CreateLoad(elemTy, rhsAddr, "cmp.r");
    if (ty.isFloating()) return env.b.CreateFCmpOEQ(L, R, "cmp");
    return env.b.CreateICmpEQ(L, R, "cmp");
  }

  if (ty.isArray() && !ty.ptrOutsideArrays) {
    if (ty.arrayDims.empty() || !ty.arrayDims[0].has_value()) {
      return llvm::ConstantInt::getTrue(env.ctx);
    }
    size_t size = *ty.arrayDims[0];
    Type elemTy = ty.elementType();
    llvm::Type* arrTy = llvmType(env, ty);
    llvm::Value* acc = nullptr;
    for (size_t i = 0; i < size; ++i) {
      llvm::Value* idxs[] = {i32Const(env, 0), i32Const(env, static_cast<int64_t>(i))};
      llvm::Value* l = env.b.CreateGEP(arrTy, lhsAddr, idxs, "arr.l");
      llvm::Value* r = env.b.CreateGEP(arrTy, rhsAddr, idxs, "arr.r");
      llvm::Value* eq = emitEqualByAddr(env, elemTy, l, r);
      acc = acc ? env.b.CreateAnd(acc, eq, "cmp.and") : eq;
    }
    if (!acc) return llvm::ConstantInt::getTrue(env.ctx);
    return acc;
  }

  if (ty.base == Type::Base::Struct && ty.ptrDepth == 0) {
    auto it = env.structFields.find(ty.structName);
    auto stIt = env.structs.find(ty.structName);
    if (it == env.structFields.end() || stIt == env.structs.end()) {
      return llvm::ConstantInt::getTrue(env.ctx);
    }
    llvm::Value* acc = nullptr;
    for (size_t i = 0; i < it->second.size(); ++i) {
      const Type& fieldTy = it->second[i].type;
      llvm::Value* l = env.b.CreateStructGEP(
          stIt->second, lhsAddr, static_cast<unsigned>(i), "fld.l");
      llvm::Value* r = env.b.CreateStructGEP(
          stIt->second, rhsAddr, static_cast<unsigned>(i), "fld.r");
      llvm::Value* eq = emitEqualByAddr(env, fieldTy, l, r);
      acc = acc ? env.b.CreateAnd(acc, eq, "cmp.and") : eq;
    }
    if (!acc) return llvm::ConstantInt::getTrue(env.ctx);
    return acc;
  }

  llvm::Type* elemTy = llvmType(env, ty);
  llvm::Value* L = env.b.CreateLoad(elemTy, lhsAddr, "cmp.l");
  llvm::Value* R = env.b.CreateLoad(elemTy, rhsAddr, "cmp.r");
  return env.b.CreateICmpEQ(L, R, "cmp");
}

static llvm::Value* emitEqual(CGEnv& env, const Type& ty, llvm::Value* lhs, llvm::Value* rhs) {
  if (ty.isPointer() || ty.isInteger()) {
    return env.b.CreateICmpEQ(lhs, rhs, "cmp");
  }
  if (ty.isFloating()) {
    return env.b.CreateFCmpOEQ(lhs, rhs, "cmp");
  }
  if (ty.isArray() || (ty.base == Type::Base::Struct && ty.ptrDepth == 0)) {
    llvm::AllocaInst* ltmp = createEntryAlloca(env, "cmp.lhs", ty);
    llvm::AllocaInst* rtmp = createEntryAlloca(env, "cmp.rhs", ty);
    env.b.CreateStore(lhs, ltmp);
    env.b.CreateStore(rhs, rtmp);
    return emitEqualByAddr(env, ty, ltmp, rtmp);
  }
  return env.b.CreateICmpEQ(lhs, rhs, "cmp");
}

static void emitInitToAddr(CGEnv& env, const Type& ty, llvm::Value* addr, const Expr& init) {
  if (auto* str = dynamic_cast<const StringLiteralExpr*>(&init)) {
    if (ty.isArray() && !ty.ptrOutsideArrays) {
      Type elemTy = ty.elementType();
      if (elemTy.base == Type::Base::Char && elemTy.ptrDepth == 0 && elemTy.arrayDims.empty()) {
        if (ty.arrayDims.empty() || !ty.arrayDims[0].has_value()) return;
        size_t size = *ty.arrayDims[0];
        llvm::Type* arrTy = llvmType(env, ty);
        for (size_t i = 0; i < size; ++i) {
          llvm::Value* idxs[] = {i32Const(env, 0), i32Const(env, static_cast<int64_t>(i))};
          llvm::Value* elemAddr = env.b.CreateGEP(arrTy, addr, idxs, "init.str");
          uint8_t ch = 0;
          if (i < str->value.size()) ch = static_cast<uint8_t>(str->value[i]);
          llvm::Value* v = llvm::ConstantInt::get(llvmType(env, elemTy), ch, false);
          env.b.CreateStore(v, elemAddr);
        }
        return;
      }
    }
  }
  if (auto* list = dynamic_cast<const InitListExpr*>(&init)) {
    if (ty.base == Type::Base::Struct && ty.ptrDepth == 0) {
      auto it = env.structFields.find(ty.structName);
      if (it == env.structFields.end()) return;
      auto stIt = env.structs.find(ty.structName);
      if (stIt == env.structs.end()) return;
      size_t count = it->second.size();
      for (size_t i = 0; i < count; ++i) {
        llvm::Value* fieldAddr = env.b.CreateStructGEP(
            stIt->second, addr, static_cast<unsigned>(i), "init.fld");
        if (i < list->elems.size()) {
          emitInitToAddr(env, it->second[i].type, fieldAddr, *list->elems[i]);
        } else {
          env.b.CreateStore(zeroValue(env, it->second[i].type), fieldAddr);
        }
      }
      return;
    }
    if (ty.isArray() && !ty.ptrOutsideArrays) {
      if (ty.arrayDims.empty() || !ty.arrayDims[0].has_value()) return;
      size_t size = *ty.arrayDims[0];
      Type elemTy = ty.elementType();
      llvm::Type* arrTy = llvmType(env, ty);
      for (size_t i = 0; i < size; ++i) {
        llvm::Value* idxs[] = {i32Const(env, 0), i32Const(env, static_cast<int64_t>(i))};
        llvm::Value* elemAddr = env.b.CreateGEP(arrTy, addr, idxs, "init.arr");
        if (i < list->elems.size()) {
          emitInitToAddr(env, elemTy, elemAddr, *list->elems[i]);
        } else {
          env.b.CreateStore(zeroValue(env, elemTy), elemAddr);
        }
      }
      return;
    }
    if (!list->elems.empty()) {
      emitInitToAddr(env, ty, addr, *list->elems[0]);
      return;
    }
    env.b.CreateStore(zeroValue(env, ty), addr);
    return;
  }

  llvm::Value* initV = emitExpr(env, init);
  if (ty.isPointer() && isNullPointerLiteral(init)) {
    llvm::Type* ptrTy = llvmType(env, ty);
    initV = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptrTy));
  } else if (ty.isPointer() && exprType(init).isPointer()) {
    llvm::Type* ptrTy = llvmType(env, ty);
    initV = castPointerIfNeeded(env, initV, ptrTy);
  } else if (ty.isNumeric() && exprType(init).isNumeric()) {
    initV = castNumericToType(env, initV, exprType(init), ty);
  }
  env.b.CreateStore(initV, addr);
}

// forward decl
static llvm::Value* emitLValue(CGEnv& env, const Expr& e);
static bool emitStmt(CGEnv& env, const Stmt& s);

// -------------------- Expr --------------------

static llvm::Value* emitUnary(CGEnv& env, const UnaryExpr& u) {
  switch (u.op) {
    case TokenKind::Amp:
      return emitLValue(env, *u.operand);
    case TokenKind::Star: {
      llvm::Value* opnd = emitExpr(env, *u.operand);
      llvm::Type* elemTy = opnd->getType()->getPointerElementType();
      return env.b.CreateLoad(elemTy, opnd, "deref");
    }
    case TokenKind::Plus: {
      llvm::Value* v = emitExpr(env, *u.operand);
      const Type& resTy = exprType(u);
      if (resTy.isNumeric()) v = castNumericToType(env, v, exprType(*u.operand), resTy);
      return v;
    }
    case TokenKind::Minus: {
      llvm::Value* v = emitExpr(env, *u.operand);
      const Type& resTy = exprType(u);
      llvm::Type* resLlvmTy = llvmType(env, resTy);
      if (resTy.isNumeric()) v = castNumericToType(env, v, exprType(*u.operand), resTy);
      if (resTy.isFloating()) {
        return env.b.CreateFNeg(v, "neg");
      }
      llvm::Value* zero = llvm::ConstantInt::get(resLlvmTy, 0, true);
      return env.b.CreateSub(zero, v, "neg");
    }
    case TokenKind::Tilde: {
      llvm::Value* v = emitExpr(env, *u.operand);
      const Type& resTy = exprType(u);
      if (resTy.isInteger()) v = castIntegerToType(env, v, exprType(*u.operand), resTy);
      return env.b.CreateNot(v, "bitnot");
    }
    case TokenKind::Bang: {
      llvm::Value* b = asBoolI1(env, emitExpr(env, *u.operand));
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

  const Type& lhsTy = exprType(*bin.lhs);
  const Type& rhsTy = exprType(*bin.rhs);

  llvm::Value* L = emitExpr(env, *bin.lhs);
  llvm::Value* R = emitExpr(env, *bin.rhs);

  switch (bin.op) {
    case TokenKind::Plus: {
      if (lhsTy.isPointer() && rhsTy.isInteger()) {
        llvm::Type* elemTy = L->getType()->getPointerElementType();
        llvm::Value* idx = castIndex(env, R, rhsTy);
        return env.b.CreateGEP(elemTy, L, idx, "ptr.add");
      }
      if (lhsTy.isInteger() && rhsTy.isPointer()) {
        llvm::Type* elemTy = R->getType()->getPointerElementType();
        llvm::Value* idx = castIndex(env, L, lhsTy);
        return env.b.CreateGEP(elemTy, R, idx, "ptr.add");
      }
      if (lhsTy.isFloating() || rhsTy.isFloating()) {
        Type resTy = commonNumericType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, resTy);
        R = castNumericToType(env, R, rhsTy, resTy);
        return env.b.CreateFAdd(L, R, "fadd");
      }
      if (lhsTy.isInteger() && rhsTy.isInteger()) {
        const Type& resTy = exprType(bin);
        L = castNumericToType(env, L, lhsTy, resTy);
        R = castNumericToType(env, R, rhsTy, resTy);
      }
      return env.b.CreateAdd(L, R, "add");
    }
    case TokenKind::Minus: {
      if (lhsTy.isPointer() && rhsTy.isInteger()) {
        llvm::Type* elemTy = L->getType()->getPointerElementType();
        llvm::Value* idx = castIndex(env, R, rhsTy);
        llvm::Value* zero = llvm::ConstantInt::get(idx->getType(), 0, true);
        llvm::Value* neg = env.b.CreateSub(zero, idx, "neg");
        return env.b.CreateGEP(elemTy, L, neg, "ptr.sub");
      }
      if (lhsTy.isPointer() && rhsTy.isPointer()) {
        llvm::Value* Li = env.b.CreatePtrToInt(L, env.b.getInt64Ty(), "ptrtoi.l");
        llvm::Value* Ri = env.b.CreatePtrToInt(R, env.b.getInt64Ty(), "ptrtoi.r");
        llvm::Value* diffBytes = env.b.CreateSub(Li, Ri, "ptrdiff.bytes");
        Type elemTy = lhsTy.pointee();
        uint64_t elemSize = (elemTy.isInteger() || elemTy.isFloating())
                                ? integerSizeBytes(elemTy)
                                : sizeof(void*);
        llvm::Value* elemSizeV = llvm::ConstantInt::get(env.b.getInt64Ty(), elemSize, false);
        llvm::Value* diffElems = env.b.CreateSDiv(diffBytes, elemSizeV, "ptrdiff");
        return env.b.CreateTrunc(diffElems, env.i32Ty(), "ptrdiff.i32");
      }
      if (lhsTy.isFloating() || rhsTy.isFloating()) {
        Type resTy = commonNumericType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, resTy);
        R = castNumericToType(env, R, rhsTy, resTy);
        return env.b.CreateFSub(L, R, "fsub");
      }
      if (lhsTy.isInteger() && rhsTy.isInteger()) {
        const Type& resTy = exprType(bin);
        L = castNumericToType(env, L, lhsTy, resTy);
        R = castNumericToType(env, R, rhsTy, resTy);
      }
      return env.b.CreateSub(L, R, "sub");
    }
    case TokenKind::Star: {
      if (lhsTy.isFloating() || rhsTy.isFloating()) {
        Type resTy = commonNumericType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, resTy);
        R = castNumericToType(env, R, rhsTy, resTy);
        return env.b.CreateFMul(L, R, "fmul");
      }
      if (lhsTy.isInteger() && rhsTy.isInteger()) {
        const Type& resTy = exprType(bin);
        L = castNumericToType(env, L, lhsTy, resTy);
        R = castNumericToType(env, R, rhsTy, resTy);
      }
      return env.b.CreateMul(L, R, "mul");
    }
    case TokenKind::Slash: {
      if (lhsTy.isFloating() || rhsTy.isFloating()) {
        Type resTy = commonNumericType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, resTy);
        R = castNumericToType(env, R, rhsTy, resTy);
        return env.b.CreateFDiv(L, R, "fdiv");
      }
      if (lhsTy.isInteger() && rhsTy.isInteger()) {
        const Type& resTy = exprType(bin);
        L = castNumericToType(env, L, lhsTy, resTy);
        R = castNumericToType(env, R, rhsTy, resTy);
        if (resTy.isUnsigned) return env.b.CreateUDiv(L, R, "udiv");
      }
      return env.b.CreateSDiv(L, R, "div");
    }
    case TokenKind::Percent: {
      if (lhsTy.isInteger() && rhsTy.isInteger()) {
        const Type& resTy = exprType(bin);
        L = castNumericToType(env, L, lhsTy, resTy);
        R = castNumericToType(env, R, rhsTy, resTy);
        if (resTy.isUnsigned) return env.b.CreateURem(L, R, "urem");
      }
      return env.b.CreateSRem(L, R, "srem");
    }
    case TokenKind::LessLess:
    case TokenKind::GreaterGreater: {
      const Type& resTy = exprType(bin);
      L = castNumericToType(env, L, lhsTy, resTy);
      R = castNumericToType(env, R, rhsTy, resTy);
      if (bin.op == TokenKind::LessLess) return env.b.CreateShl(L, R, "shl");
      if (resTy.isUnsigned) return env.b.CreateLShr(L, R, "lshr");
      return env.b.CreateAShr(L, R, "ashr");
    }
    case TokenKind::Amp:
    case TokenKind::Pipe:
    case TokenKind::Caret: {
      const Type& resTy = exprType(bin);
      L = castNumericToType(env, L, lhsTy, resTy);
      R = castNumericToType(env, R, rhsTy, resTy);
      if (bin.op == TokenKind::Amp) return env.b.CreateAnd(L, R, "and");
      if (bin.op == TokenKind::Pipe) return env.b.CreateOr(L, R, "or");
      return env.b.CreateXor(L, R, "xor");
    }

    case TokenKind::Comma:
      return R;

    case TokenKind::Less: {
      llvm::Value* c = nullptr;
      if (lhsTy.isPointer()) {
        c = env.b.CreateICmpULT(L, R, "cmp");
      } else if (lhsTy.isFloating() || rhsTy.isFloating()) {
        Type cmpTy = commonNumericType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
        c = env.b.CreateFCmpOLT(L, R, "cmp");
      } else if (lhsTy.isInteger() && rhsTy.isInteger()) {
        Type cmpTy = commonIntegerType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
        c = cmpTy.isUnsigned ? env.b.CreateICmpULT(L, R, "cmp")
                             : env.b.CreateICmpSLT(L, R, "cmp");
      } else {
        c = env.b.CreateICmpSLT(L, R, "cmp");
      }
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }
    case TokenKind::LessEqual: {
      llvm::Value* c = nullptr;
      if (lhsTy.isPointer()) {
        c = env.b.CreateICmpULE(L, R, "cmp");
      } else if (lhsTy.isFloating() || rhsTy.isFloating()) {
        Type cmpTy = commonNumericType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
        c = env.b.CreateFCmpOLE(L, R, "cmp");
      } else if (lhsTy.isInteger() && rhsTy.isInteger()) {
        Type cmpTy = commonIntegerType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
        c = cmpTy.isUnsigned ? env.b.CreateICmpULE(L, R, "cmp")
                             : env.b.CreateICmpSLE(L, R, "cmp");
      } else {
        c = env.b.CreateICmpSLE(L, R, "cmp");
      }
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }
    case TokenKind::Greater: {
      llvm::Value* c = nullptr;
      if (lhsTy.isPointer()) {
        c = env.b.CreateICmpUGT(L, R, "cmp");
      } else if (lhsTy.isFloating() || rhsTy.isFloating()) {
        Type cmpTy = commonNumericType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
        c = env.b.CreateFCmpOGT(L, R, "cmp");
      } else if (lhsTy.isInteger() && rhsTy.isInteger()) {
        Type cmpTy = commonIntegerType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
        c = cmpTy.isUnsigned ? env.b.CreateICmpUGT(L, R, "cmp")
                             : env.b.CreateICmpSGT(L, R, "cmp");
      } else {
        c = env.b.CreateICmpSGT(L, R, "cmp");
      }
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }
    case TokenKind::GreaterEqual: {
      llvm::Value* c = nullptr;
      if (lhsTy.isPointer()) {
        c = env.b.CreateICmpUGE(L, R, "cmp");
      } else if (lhsTy.isFloating() || rhsTy.isFloating()) {
        Type cmpTy = commonNumericType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
        c = env.b.CreateFCmpOGE(L, R, "cmp");
      } else if (lhsTy.isInteger() && rhsTy.isInteger()) {
        Type cmpTy = commonIntegerType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
        c = cmpTy.isUnsigned ? env.b.CreateICmpUGE(L, R, "cmp")
                             : env.b.CreateICmpSGE(L, R, "cmp");
      } else {
        c = env.b.CreateICmpSGE(L, R, "cmp");
      }
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }
    case TokenKind::EqualEqual: {
      if (lhsTy.base == Type::Base::Struct && lhsTy.ptrDepth == 0 && lhsTy == rhsTy) {
        llvm::Value* eq = emitEqual(env, lhsTy, L, R);
        return env.b.CreateZExt(eq, env.i32Ty(), "cmp.i32");
      }
      if (lhsTy.isFloating() || rhsTy.isFloating()) {
        Type cmpTy = commonNumericType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
        auto* c = env.b.CreateFCmpOEQ(L, R, "cmp");
        return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
      }
      if (lhsTy.isInteger() && rhsTy.isInteger()) {
        Type cmpTy = commonIntegerType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
      }
      if (lhsTy.isPointer() && rhsTy.isInt() && isNullPointerLiteral(*bin.rhs)) {
        auto* pty = llvm::cast<llvm::PointerType>(L->getType());
        R = llvm::ConstantPointerNull::get(pty);
      } else if (rhsTy.isPointer() && lhsTy.isInt() && isNullPointerLiteral(*bin.lhs)) {
        auto* pty = llvm::cast<llvm::PointerType>(R->getType());
        L = llvm::ConstantPointerNull::get(pty);
      } else if (L->getType()->isPointerTy() && R->getType()->isPointerTy() &&
                 L->getType() != R->getType()) {
        llvm::Type* i8ptr = llvm::Type::getInt8PtrTy(env.ctx);
        L = castPointerIfNeeded(env, L, i8ptr);
        R = castPointerIfNeeded(env, R, i8ptr);
      }
      auto* c = env.b.CreateICmpEQ(L, R, "cmp");
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }
    case TokenKind::BangEqual: {
      if (lhsTy.base == Type::Base::Struct && lhsTy.ptrDepth == 0 && lhsTy == rhsTy) {
        llvm::Value* eq = emitEqual(env, lhsTy, L, R);
        llvm::Value* ne = env.b.CreateNot(eq, "cmp.not");
        return env.b.CreateZExt(ne, env.i32Ty(), "cmp.i32");
      }
      if (lhsTy.isFloating() || rhsTy.isFloating()) {
        Type cmpTy = commonNumericType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
        auto* c = env.b.CreateFCmpONE(L, R, "cmp");
        return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
      }
      if (lhsTy.isInteger() && rhsTy.isInteger()) {
        Type cmpTy = commonIntegerType(lhsTy, rhsTy);
        L = castNumericToType(env, L, lhsTy, cmpTy);
        R = castNumericToType(env, R, rhsTy, cmpTy);
      }
      if (lhsTy.isPointer() && rhsTy.isInt() && isNullPointerLiteral(*bin.rhs)) {
        auto* pty = llvm::cast<llvm::PointerType>(L->getType());
        R = llvm::ConstantPointerNull::get(pty);
      } else if (rhsTy.isPointer() && lhsTy.isInt() && isNullPointerLiteral(*bin.lhs)) {
        auto* pty = llvm::cast<llvm::PointerType>(R->getType());
        L = llvm::ConstantPointerNull::get(pty);
      } else if (L->getType()->isPointerTy() && R->getType()->isPointerTy() &&
                 L->getType() != R->getType()) {
        llvm::Type* i8ptr = llvm::Type::getInt8PtrTy(env.ctx);
        L = castPointerIfNeeded(env, L, i8ptr);
        R = castPointerIfNeeded(env, R, i8ptr);
      }
      auto* c = env.b.CreateICmpNE(L, R, "cmp");
      return env.b.CreateZExt(c, env.i32Ty(), "cmp.i32");
    }

    default:
      return i32Const(env, 0);
  }
}

static llvm::Value* emitLValue(CGEnv& env, const Expr& e) {
  if (auto* vr = dynamic_cast<const VarRefExpr*>(&e)) {
    if (auto* local = env.lookupLocal(vr->name)) return local->slot;
    if (auto* global = env.lookupGlobal(vr->name)) return global->gv;
    return nullptr;
  }

  if (auto* un = dynamic_cast<const UnaryExpr*>(&e)) {
    if (un->op == TokenKind::Star) return emitExpr(env, *un->operand);
  }

  if (auto* sub = dynamic_cast<const SubscriptExpr*>(&e)) {
    llvm::Value* basePtr = emitExpr(env, *sub->base);
    llvm::Value* idx = emitExpr(env, *sub->index);
    Type baseTy = exprType(*sub->base);
    if (baseTy.isArray() && !baseTy.ptrOutsideArrays) baseTy = baseTy.decayType();
    Type elemTy = baseTy.pointee();
    llvm::Type* llvmElemTy = llvmType(env, elemTy);
    Type idxTy = exprType(*sub->index);
    llvm::Value* adjIdx = castIndex(env, idx, idxTy);
    return env.b.CreateGEP(llvmElemTy, basePtr, adjIdx, "sub.addr");
  }

  if (auto* mem = dynamic_cast<const MemberExpr*>(&e)) {
    Type baseTy = exprType(*mem->base);
    Type structTy = baseTy;
    llvm::Value* basePtr = nullptr;
    if (mem->isArrow) {
      basePtr = emitExpr(env, *mem->base);
      structTy = baseTy.pointee();
    } else {
      basePtr = emitLValue(env, *mem->base);
    }

    auto it = env.structFields.find(structTy.structName);
    if (it == env.structFields.end()) return nullptr;
    size_t fieldIndex = 0;
    bool found = false;
    for (size_t i = 0; i < it->second.size(); ++i) {
      if (it->second[i].name == mem->member) {
        fieldIndex = i;
        found = true;
        break;
      }
    }
    if (!found) return nullptr;
    auto stIt = env.structs.find(structTy.structName);
    if (stIt == env.structs.end()) return nullptr;
    return env.b.CreateStructGEP(stIt->second, basePtr,
                                 static_cast<unsigned>(fieldIndex), "member.addr");
  }

  return nullptr;
}

static llvm::Value* emitExpr(CGEnv& env, const Expr& e) {
  if (auto* lit = dynamic_cast<const IntLiteralExpr*>(&e)) {
    return i32Const(env, lit->value);
  }
  if (auto* flt = dynamic_cast<const FloatLiteralExpr*>(&e)) {
    llvm::Type* ty = flt->isFloat ? llvm::Type::getFloatTy(env.ctx)
                                  : llvm::Type::getDoubleTy(env.ctx);
    return llvm::ConstantFP::get(ty, flt->value);
  }

  if (auto* str = dynamic_cast<const StringLiteralExpr*>(&e)) {
    return emitStringLiteral(env, str->value);
  }

  if (auto* inc = dynamic_cast<const IncDecExpr*>(&e)) {
    llvm::Value* addr = emitLValue(env, *inc->operand);
    if (!addr) return i32Const(env, 0);
    Type opTy = exprType(*inc->operand);
    llvm::Value* oldV = env.b.CreateLoad(llvmType(env, opTy), addr, "incdec.old");
    llvm::Value* newV = nullptr;
    if (opTy.isPointer()) {
      llvm::Type* elemTy = oldV->getType()->getPointerElementType();
      llvm::Value* idx = llvm::ConstantInt::get(env.b.getInt64Ty(), inc->isInc ? 1 : -1, true);
      newV = env.b.CreateGEP(elemTy, oldV, idx, "incdec.ptr");
    } else {
      llvm::Value* one = llvm::ConstantInt::get(oldV->getType(), 1, true);
      newV = inc->isInc ? env.b.CreateAdd(oldV, one, "incdec.add")
                        : env.b.CreateSub(oldV, one, "incdec.sub");
    }
    env.b.CreateStore(newV, addr);
    return inc->isPost ? oldV : newV;
  }

  if (auto* sz = dynamic_cast<const SizeofExpr*>(&e)) {
    Type t = sz->isType ? sz->type : exprType(*sz->expr);
    uint64_t size = sizeOfType(t, env);
    return i32Const(env, static_cast<int64_t>(size));
  }

  if (auto* cast = dynamic_cast<const CastExpr*>(&e)) {
    llvm::Value* v = emitExpr(env, *cast->expr);
    const Type& srcTy = exprType(*cast->expr);
    const Type& dstTy = cast->targetType;
    if (dstTy.isPointer()) {
      llvm::Type* llvmDst = llvmType(env, dstTy);
      if (srcTy.isPointer()) return castPointerIfNeeded(env, v, llvmDst);
      if (srcTy.isInteger()) return env.b.CreateIntToPtr(v, llvmDst, "int.to.ptr");
      return v;
    }
    if (dstTy.isInteger()) {
      if (srcTy.isPointer()) {
        llvm::Value* asInt = env.b.CreatePtrToInt(v, env.b.getInt64Ty(), "ptr.to.int");
        return castIntegerToType(env, asInt, Type{Type::Base::LongLong, 0}, dstTy);
      }
      if (srcTy.isNumeric()) return castNumericToType(env, v, srcTy, dstTy);
      return v;
    }
    if (dstTy.isFloating()) {
      if (srcTy.isNumeric()) return castNumericToType(env, v, srcTy, dstTy);
      return v;
    }
    return v;
  }

  if (auto* vr = dynamic_cast<const VarRefExpr*>(&e)) {
    if (auto* local = env.lookupLocal(vr->name)) {
      if (local->type.isArray() && !local->type.ptrOutsideArrays) {
        return decayArrayToPointer(env, local->slot, local->type);
      }
      return env.b.CreateLoad(llvmType(env, local->type), local->slot, vr->name + ".val");
    }
    if (auto* global = env.lookupGlobal(vr->name)) {
      if (global->type.isArray() && !global->type.ptrOutsideArrays) {
        return decayArrayToPointer(env, global->gv, global->type);
      }
      return env.b.CreateLoad(llvmType(env, global->type), global->gv, vr->name + ".gval");
    }
    auto it = env.enumConstants.find(vr->name);
    if (it != env.enumConstants.end()) {
      return i32Const(env, it->second);
    }
    return i32Const(env, 0);
  }

  if (auto* call = dynamic_cast<const CallExpr*>(&e)) {
    llvm::Function* callee = nullptr;
    auto it = env.functions.find(call->callee);
    if (it != env.functions.end()) callee = it->second;
    if (!callee) return i32Const(env, 0);

    const std::vector<Type>* paramTypes = nullptr;
    auto pit = env.functionParamTypes.find(call->callee);
    if (pit != env.functionParamTypes.end()) paramTypes = &pit->second;

    std::vector<llvm::Value*> argsV;
    argsV.reserve(call->args.size());
    for (size_t i = 0; i < call->args.size(); ++i) {
      const auto& a = call->args[i];
      if (paramTypes && i < paramTypes->size()) {
        const Type& dstTy = (*paramTypes)[i];
        llvm::Type* paramTy = llvmType(env, dstTy);
        if (dstTy.isPointer() && isNullPointerLiteral(*a)) {
          argsV.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(paramTy)));
          continue;
        }
        if (dstTy.isPointer() && exprType(*a).isPointer()) {
          llvm::Value* v = emitExpr(env, *a);
          argsV.push_back(castPointerIfNeeded(env, v, paramTy));
          continue;
        }
        if (dstTy.isNumeric() && exprType(*a).isNumeric()) {
          llvm::Value* v = emitExpr(env, *a);
          argsV.push_back(castNumericToType(env, v, exprType(*a), dstTy));
          continue;
        }
      }
      argsV.push_back(emitExpr(env, *a));
    }

    return env.b.CreateCall(callee, argsV, "calltmp");
  }

  if (auto* ter = dynamic_cast<const TernaryExpr*>(&e)) {
    llvm::Value* condV = emitExpr(env, *ter->cond);
    llvm::Value* condB = asBoolI1(env, condV);

    llvm::Function* F = env.fn;
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(env.ctx, "ternary.then", F);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(env.ctx, "ternary.else", F);
    llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(env.ctx, "ternary.end", F);

    env.b.CreateCondBr(condB, thenBB, elseBB);

    env.b.SetInsertPoint(thenBB);
    llvm::Value* thenV = emitExpr(env, *ter->thenExpr);
    if (!env.b.GetInsertBlock()->getTerminator()) env.b.CreateBr(endBB);
    thenBB = env.b.GetInsertBlock();

    env.b.SetInsertPoint(elseBB);
    llvm::Value* elseV = emitExpr(env, *ter->elseExpr);
    if (!env.b.GetInsertBlock()->getTerminator()) env.b.CreateBr(endBB);
    elseBB = env.b.GetInsertBlock();

    env.b.SetInsertPoint(endBB);
    const Type& resTy = exprType(*ter);
    llvm::Type* resLlvmTy = llvmType(env, resTy);
    if (resTy.isPointer()) {
      if (isNullPointerLiteral(*ter->thenExpr)) {
        thenV = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(resLlvmTy));
      }
      if (isNullPointerLiteral(*ter->elseExpr)) {
        elseV = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(resLlvmTy));
      }
    } else if (resTy.isNumeric()) {
      thenV = castNumericToType(env, thenV, exprType(*ter->thenExpr), resTy);
      elseV = castNumericToType(env, elseV, exprType(*ter->elseExpr), resTy);
    }
    llvm::PHINode* phi = env.b.CreatePHI(resLlvmTy, 2, "ternary");
    phi->addIncoming(thenV, thenBB);
    phi->addIncoming(elseV, elseBB);
    return phi;
  }

  if (auto* un = dynamic_cast<const UnaryExpr*>(&e)) {
    return emitUnary(env, *un);
  }

  if (auto* sub = dynamic_cast<const SubscriptExpr*>(&e)) {
    llvm::Value* addr = emitLValue(env, *sub);
    const Type& elemTy = exprType(e);
    if (elemTy.isArray()) {
      return decayArrayToPointer(env, addr, elemTy);
    }
    return env.b.CreateLoad(llvmType(env, elemTy), addr, "sub.val");
  }

  if (auto* mem = dynamic_cast<const MemberExpr*>(&e)) {
    llvm::Value* addr = emitLValue(env, *mem);
    Type baseTy = exprType(*mem->base);
    Type structTy = mem->isArrow ? baseTy.pointee() : baseTy;
    Type fieldTy;
    bool found = false;
    auto it = env.structFields.find(structTy.structName);
    if (it != env.structFields.end()) {
      for (const auto& field : it->second) {
        if (field.name == mem->member) {
          fieldTy = field.type;
          found = true;
          break;
        }
      }
    }
    const Type& elemTy = found ? fieldTy : exprType(e);
    if (elemTy.isArray() && !elemTy.ptrOutsideArrays) {
      return decayArrayToPointer(env, addr, elemTy);
    }
    return env.b.CreateLoad(llvmType(env, elemTy), addr, "member.val");
  }

  if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
    return emitBinary(env, *bin);
  }

  if (auto* asn = dynamic_cast<const AssignExpr*>(&e)) {
    llvm::Value* addr = emitLValue(env, *asn->lhs);
    const Type& lhsTy = exprType(*asn->lhs);
    const Type& rhsTy = exprType(*asn->rhs);
    llvm::Value* rhsV = emitExpr(env, *asn->rhs);

    if (asn->op != TokenKind::Assign) {
      llvm::Value* lhsV = env.b.CreateLoad(llvmType(env, lhsTy), addr, "assign.lhs");
      llvm::Value* newV = nullptr;
      Type resultTy = lhsTy;
      if (asn->op == TokenKind::PlusAssign || asn->op == TokenKind::MinusAssign) {
        if (lhsTy.isPointer() && rhsTy.isInteger()) {
          llvm::Value* idx = castIndex(env, rhsV, rhsTy);
          if (asn->op == TokenKind::MinusAssign) {
            llvm::Value* zero = llvm::ConstantInt::get(idx->getType(), 0, true);
            idx = env.b.CreateSub(zero, idx, "neg");
          }
          llvm::Type* elemTy = lhsV->getType()->getPointerElementType();
          newV = env.b.CreateGEP(elemTy, lhsV, idx, "ptr.add");
        } else {
          Type resTy = commonNumericType(lhsTy, rhsTy);
          resultTy = resTy;
          llvm::Value* L = castNumericToType(env, lhsV, lhsTy, resTy);
          llvm::Value* R = castNumericToType(env, rhsV, rhsTy, resTy);
          if (resTy.isFloating()) {
            newV = (asn->op == TokenKind::PlusAssign)
                      ? env.b.CreateFAdd(L, R, "fadd")
                      : env.b.CreateFSub(L, R, "fsub");
          } else {
            newV = (asn->op == TokenKind::PlusAssign)
                      ? env.b.CreateAdd(L, R, "add")
                      : env.b.CreateSub(L, R, "sub");
          }
        }
      } else if (asn->op == TokenKind::StarAssign || asn->op == TokenKind::SlashAssign) {
        Type resTy = commonNumericType(lhsTy, rhsTy);
        resultTy = resTy;
        llvm::Value* L = castNumericToType(env, lhsV, lhsTy, resTy);
        llvm::Value* R = castNumericToType(env, rhsV, rhsTy, resTy);
        if (resTy.isFloating()) {
          newV = (asn->op == TokenKind::StarAssign)
                    ? env.b.CreateFMul(L, R, "fmul")
                    : env.b.CreateFDiv(L, R, "fdiv");
        } else {
          newV = (asn->op == TokenKind::StarAssign)
                    ? env.b.CreateMul(L, R, "mul")
                    : (resTy.isUnsigned ? env.b.CreateUDiv(L, R, "udiv")
                                        : env.b.CreateSDiv(L, R, "sdiv"));
        }
      } else if (asn->op == TokenKind::PercentAssign) {
        Type resTy = commonIntegerType(lhsTy, rhsTy);
        resultTy = resTy;
        llvm::Value* L = castNumericToType(env, lhsV, lhsTy, resTy);
        llvm::Value* R = castNumericToType(env, rhsV, rhsTy, resTy);
        newV = resTy.isUnsigned ? env.b.CreateURem(L, R, "urem")
                                : env.b.CreateSRem(L, R, "srem");
      } else if (asn->op == TokenKind::LessLessAssign || asn->op == TokenKind::GreaterGreaterAssign) {
        Type resTy = promoteInteger(lhsTy);
        resultTy = resTy;
        llvm::Value* L = castNumericToType(env, lhsV, lhsTy, resTy);
        llvm::Value* R = castNumericToType(env, rhsV, rhsTy, resTy);
        if (asn->op == TokenKind::LessLessAssign) {
          newV = env.b.CreateShl(L, R, "shl");
        } else if (resTy.isUnsigned) {
          newV = env.b.CreateLShr(L, R, "lshr");
        } else {
          newV = env.b.CreateAShr(L, R, "ashr");
        }
      } else if (asn->op == TokenKind::AmpAssign || asn->op == TokenKind::PipeAssign ||
                 asn->op == TokenKind::CaretAssign) {
        Type resTy = commonIntegerType(lhsTy, rhsTy);
        resultTy = resTy;
        llvm::Value* L = castNumericToType(env, lhsV, lhsTy, resTy);
        llvm::Value* R = castNumericToType(env, rhsV, rhsTy, resTy);
        if (asn->op == TokenKind::AmpAssign) newV = env.b.CreateAnd(L, R, "and");
        else if (asn->op == TokenKind::PipeAssign) newV = env.b.CreateOr(L, R, "or");
        else newV = env.b.CreateXor(L, R, "xor");
      }

      if (!newV) return i32Const(env, 0);
      llvm::Value* storeV = newV;
      if (lhsTy.isNumeric() && storeV->getType() != llvmType(env, lhsTy)) {
        storeV = castNumericToType(env, storeV, resultTy, lhsTy);
      }
      env.b.CreateStore(storeV, addr);
      return storeV;
    }

    if (lhsTy.isPointer() && isNullPointerLiteral(*asn->rhs)) {
      llvm::Type* ptrTy = llvmType(env, lhsTy);
      rhsV = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptrTy));
    } else if (lhsTy.isPointer() && rhsTy.isPointer()) {
      llvm::Type* ptrTy = llvmType(env, lhsTy);
      rhsV = castPointerIfNeeded(env, rhsV, ptrTy);
    } else if (lhsTy.isNumeric() && rhsTy.isNumeric()) {
      rhsV = castNumericToType(env, rhsV, rhsTy, lhsTy);
    }
    env.b.CreateStore(rhsV, addr);
    return rhsV;
  }

  return i32Const(env, 0);
}

// -------------------- Stmt --------------------

static bool emitStmt(CGEnv& env, const Stmt& s);

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

static bool emitSwitch(CGEnv& env, const SwitchStmt& s) {
  llvm::Function* F = env.fn;

  llvm::Value* condV = emitExpr(env, *s.cond);
  if (condV->getType()->isIntegerTy() && !condV->getType()->isIntegerTy(32)) {
    Type condTy = exprType(*s.cond);
    Type dstTy;
    dstTy.base = Type::Base::Int;
    dstTy.isUnsigned = condTy.isUnsigned;
    condV = castNumericToType(env, condV, condTy, dstTy);
  }
  llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(env.ctx, "switch.end", F);

  std::vector<llvm::BasicBlock*> caseBBs;
  caseBBs.reserve(s.cases.size());

  llvm::BasicBlock* defaultBB = endBB;
  for (const auto& c : s.cases) {
    llvm::BasicBlock* bb = llvm::BasicBlock::Create(env.ctx, "switch.case", F);
    caseBBs.push_back(bb);
    if (!c.value.has_value()) defaultBB = bb;
  }

  llvm::SwitchInst* sw = env.b.CreateSwitch(condV, defaultBB, s.cases.size());
  for (size_t i = 0; i < s.cases.size(); i++) {
    if (!s.cases[i].value.has_value()) continue;
    int64_t v = *s.cases[i].value;
    sw->addCase(llvm::ConstantInt::get(llvm::Type::getInt32Ty(env.ctx), (uint64_t)v, true),
                caseBBs[i]);
  }

  env.pushScope();
  env.loops.push_back({endBB, nullptr}); // break target only

  for (size_t i = 0; i < s.cases.size(); i++) {
    env.b.SetInsertPoint(caseBBs[i]);
    bool terminated = false;
    for (const auto& st : s.cases[i].stmts) {
      terminated = emitStmt(env, *st);
      if (terminated) break;
    }

    if (!env.b.GetInsertBlock()->getTerminator()) {
      llvm::BasicBlock* nextBB = (i + 1 < caseBBs.size()) ? caseBBs[i + 1] : endBB;
      env.b.CreateBr(nextBB);
    }
  }

  env.loops.pop_back();
  env.popScope();

  env.b.SetInsertPoint(endBB);
  return false;
}

static bool emitIf(CGEnv& env, const IfStmt& s) {
  llvm::Function* F = env.fn;

  llvm::Value* condV = emitExpr(env, *s.cond);
  llvm::Value* condB = asBoolI1(env, condV);

  llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(env.ctx, "if.then", F);
  llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(env.ctx, "if.else");
  llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(env.ctx, "if.end");

  if (s.elseBranch) {
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
    for (const auto& item : d->items) {
      llvm::AllocaInst* slot = createEntryAlloca(env, item.name, item.type);
      env.insertLocal(item.name, slot, item.type);
      if (item.initExpr) {
        emitInitToAddr(env, item.type, slot, *item.initExpr);
      } else {
        env.b.CreateStore(zeroValue(env, item.type), slot);
      }
    }
    return false;
  }

  // legacy AssignStmt
  if (auto* a = dynamic_cast<const AssignStmt*>(&s)) {
    llvm::Value* rhsV = emitExpr(env, *a->valueExpr);
    if (auto* local = env.lookupLocal(a->name)) {
      env.b.CreateStore(rhsV, local->slot);
    } else if (auto* global = env.lookupGlobal(a->name)) {
      env.b.CreateStore(rhsV, global->gv);
    }
    return false;
  }

  if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
    llvm::Value* retV = emitExpr(env, *r->valueExpr);
    if (env.currentReturnType.isPointer() && isNullPointerLiteral(*r->valueExpr)) {
      llvm::Type* ptrTy = llvmType(env, env.currentReturnType);
      retV = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptrTy));
    } else if (env.currentReturnType.isPointer() && exprType(*r->valueExpr).isPointer()) {
      llvm::Type* ptrTy = llvmType(env, env.currentReturnType);
      retV = castPointerIfNeeded(env, retV, ptrTy);
    } else if (env.currentReturnType.isNumeric() && exprType(*r->valueExpr).isNumeric()) {
      retV = castNumericToType(env, retV, exprType(*r->valueExpr), env.currentReturnType);
    }
    env.b.CreateRet(retV);
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
    for (auto it = env.loops.rbegin(); it != env.loops.rend(); ++it) {
      if (it->second) {
        env.b.CreateBr(it->second);
        return true;
      }
    }
    return false;
  }

  if (auto* sw = dynamic_cast<const SwitchStmt*>(&s)) return emitSwitch(env, *sw);
  if (auto* iff = dynamic_cast<const IfStmt*>(&s)) return emitIf(env, *iff);
  if (auto* wh  = dynamic_cast<const WhileStmt*>(&s)) return emitWhile(env, *wh);
  if (auto* dw  = dynamic_cast<const DoWhileStmt*>(&s)) return emitDoWhile(env, *dw);
  if (auto* fo  = dynamic_cast<const ForStmt*>(&s)) return emitFor(env, *fo);

  if (auto* es = dynamic_cast<const ExprStmt*>(&s)) {
    (void)emitExpr(env, *es->expr);
    return false;
  }

  if (dynamic_cast<const TypedefStmt*>(&s)) return false;

  if (dynamic_cast<const EmptyStmt*>(&s)) return false;

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

  std::vector<std::pair<llvm::GlobalVariable*, const Expr*>> globalInits;

  for (const auto& item : tu.items) {
    auto* ed = std::get_if<EnumDef>(&item);
    if (!ed) continue;
    for (const auto& it : ed->items) {
      env.enumConstants.emplace(it.name, it.value);
    }
  }

  // 0) Predeclare struct types
  for (const auto& item : tu.items) {
    auto* sd = std::get_if<StructDef>(&item);
    if (!sd) continue;
    if (env.structs.count(sd->name)) continue;
    env.structs.emplace(sd->name, llvm::StructType::create(ctx, sd->name));
    env.structFields.emplace(sd->name, sd->fields);
  }

  for (const auto& item : tu.items) {
    auto* sd = std::get_if<StructDef>(&item);
    if (!sd) continue;
    auto it = env.structs.find(sd->name);
    if (it == env.structs.end()) continue;
    std::vector<llvm::Type*> fieldTys;
    fieldTys.reserve(sd->fields.size());
    for (const auto& field : sd->fields) {
      fieldTys.push_back(llvmType(env, field.type));
    }
    it->second->setBody(fieldTys, /*isPacked=*/false);
  }

  // 1) Emit global variables
  for (const auto& item : tu.items) {
    auto* g = std::get_if<GlobalVarDecl>(&item);
    if (!g) continue;
    for (const auto& decl : g->items) {
      llvm::Type* gvTy = llvmType(env, decl.type);
      llvm::Constant* init = nullptr;
      if (decl.type.isArray()) {
        init = llvm::ConstantAggregateZero::get(gvTy);
      } else if (decl.type.base == Type::Base::Struct && decl.type.ptrDepth == 0) {
        init = llvm::ConstantAggregateZero::get(gvTy);
      } else if (decl.type.isPointer()) {
        init = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(gvTy));
      } else {
        init = zeroValue(env, decl.type);
      }
      auto* gv = new llvm::GlobalVariable(
          *mod, gvTy, /*isConstant=*/false, llvm::GlobalValue::ExternalLinkage, init, decl.name);
      env.insertGlobal(decl.name, gv, decl.type);
      if (decl.initExpr) globalInits.emplace_back(gv, decl.initExpr.get());
    }
  }

  // 2) Predeclare all functions from prototypes (decls + defs)
  for (const auto& item : tu.items) {
    const FunctionProto* p = getProto(item);
    if (!p) continue;

    const std::string& name = p->name;

    // if already declared, skip (Sema guarantees signature consistency)
    if (env.functions.count(name)) continue;

    std::vector<llvm::Type*> paramTys;
    paramTys.reserve(p->params.size());
    std::vector<Type> paramTypes;
    paramTypes.reserve(p->params.size());
    for (const auto& prm : p->params) {
      Type adj = adjustParamType(prm.type);
      paramTypes.push_back(adj);
      paramTys.push_back(llvmType(env, adj));
    }
    auto* fnTy = llvm::FunctionType::get(llvmType(env, p->returnType), paramTys, false);
    llvm::Function* F = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, name, mod.get());
    env.functions[name] = F;
    env.functionParamTypes.emplace(name, std::move(paramTypes));

    // name args if we have parameter names (definition may have names even if earlier decl didn't)
    unsigned i = 0;
    for (auto& arg : F->args()) {
      if (i < p->params.size() && p->params[i].name.has_value()) arg.setName(*p->params[i].name);
      ++i;
    }
  }

  llvm::Function* initFn = nullptr;
  if (!globalInits.empty()) {
    auto* initTy = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false);
    initFn = llvm::Function::Create(
        initTy, llvm::GlobalValue::InternalLinkage, "__c99cc_init_globals", mod.get());

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", initFn);
    builder.SetInsertPoint(entry);
    env.resetFunctionState(initFn);

    for (const auto& gi : globalInits) {
      auto* binding = env.lookupGlobal(gi.first->getName().str());
      if (binding) {
        emitInitToAddr(env, binding->type, gi.first, *gi.second);
      }
    }

    builder.CreateRetVoid();
    llvm::verifyFunction(*initFn);
  }

  // 3) Emit bodies only for FunctionDef
  for (const auto& item : tu.items) {
    auto* def = std::get_if<FunctionDef>(&item);
    if (!def) continue;

    const FunctionProto& p = def->proto;
    llvm::Function* F = env.functions[p.name];
    assert(F && "function must have been declared");

    if (!F->empty()) {
      // already emitted (shouldn't happen if Sema prevents redefinition)
      continue;
    }

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(ctx, "entry", F);
    builder.SetInsertPoint(entry);

    env.resetFunctionState(F);
    env.currentReturnType = p.returnType;
    env.pushScope(); // function scope

    // lower parameters: allocate only those with names; still need to accept unnamed params
    unsigned idx = 0;
    for (auto& arg : F->args()) {
      if (idx < p.params.size() && p.params[idx].name.has_value()) {
        std::string pname = *p.params[idx].name;
        Type prmTy = adjustParamType(p.params[idx].type);
        llvm::AllocaInst* slot = createEntryAlloca(env, pname, prmTy);
        env.insertLocal(pname, slot, prmTy);
        builder.CreateStore(&arg, slot);
      }
      ++idx;
    }

    if (initFn && p.name == "main") {
      builder.CreateCall(initFn);
    }

    bool terminated = false;
    for (const auto& st : def->body) {
      terminated = emitStmt(env, *st);
      if (terminated) break;
    }

    if (!builder.GetInsertBlock()->getTerminator()) {
      if (p.returnType.isPointer()) {
        llvm::Type* ptrTy = llvmType(env, p.returnType);
        builder.CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptrTy)));
      } else {
        builder.CreateRet(zeroValue(env, p.returnType));
      }
    }

    env.popScope();
    llvm::verifyFunction(*F);
  }

  return mod;
}

} // namespace c99cc
