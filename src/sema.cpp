#include "sema.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace c99cc {

namespace {

using Scope = std::unordered_map<std::string, Type>;
using ScopeStack = std::vector<Scope>;
using EnumConstTable = std::unordered_map<std::string, int64_t>;
using EnumTypeTable = std::unordered_set<std::string>;

static std::optional<Type> lookupVarType(const ScopeStack& scopes, const std::string& name) {
  for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) return found->second;
  }
  return std::nullopt;
}

// ---- function table ----

struct FnInfo {
  std::vector<Type> paramTypes;
  Type returnType;
  bool isVariadic = false;
  bool hasDecl = false;
  bool hasDef = false;
  bool isStatic = false;
  SourceLocation firstLoc{};
};

using FnTable = std::unordered_map<std::string, FnInfo>;

struct StructInfo {
  std::vector<StructField> fields;
  SourceLocation nameLoc{};
};

using StructTable = std::unordered_map<std::string, StructInfo>;

static const StructInfo* lookupStruct(const StructTable& structs, const std::string& name) {
  auto it = structs.find(name);
  if (it == structs.end()) return nullptr;
  return &it->second;
}

static Type adjustParamType(const Type& t);

static bool sameSignature(const FnInfo& info, const FunctionProto& proto) {
  if (info.paramTypes.size() != proto.params.size()) return false;
  if (info.returnType != proto.returnType) return false;
  if (info.isVariadic != proto.isVariadic) return false;
  for (size_t i = 0; i < proto.params.size(); ++i) {
    if (info.paramTypes[i] != adjustParamType(proto.params[i].type)) return false;
  }
  return true;
}

static bool isNullPointerConstant(const Expr& e) {
  if (auto* lit = dynamic_cast<const IntLiteralExpr*>(&e)) return lit->value == 0;
  return false;
}

static Type functionPointerTypeFromFnInfo(const FnInfo& info) {
  Type t = info.returnType;
  t.ptrDepth = 0;
  t.ptrConst.clear();
  t.addPointerLevel(false);
  t.arrayDims.clear();
  t.ptrOutsideArrays = false;
  auto fnTy = std::make_shared<FunctionType>();
  fnTy->returnType = info.returnType;
  fnTy->params = info.paramTypes;
  fnTy->isVariadic = info.isVariadic;
  t.func = std::move(fnTy);
  return t;
}

// ---- expr/stmt checking ----

static std::optional<Type> checkExprImpl(
    Diagnostics& diags, ScopeStack& scopes, const FnTable& fns, const StructTable& structs,
    const EnumConstTable& enums, Expr& e);

static bool isScalarType(const Type& t) {
  return t.isNumeric() || t.isPointer();
}

static const StringLiteralExpr* asStringLiteral(const Expr& e) {
  return dynamic_cast<const StringLiteralExpr*>(&e);
}

static bool isAssignable(const Type& dst, const Type& src, const Expr& srcExpr);

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

static Type commonIntegerType(const Type& lhs, const Type& rhs) {
  Type L = promoteInteger(lhs);
  Type R = promoteInteger(rhs);
  int rank = std::max(integerRank(L), integerRank(R));
  Type res = typeFromRank(rank);
  res.isUnsigned = L.isUnsigned || R.isUnsigned;
  return res;
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

static bool isValidUnsignedUse(const Type& t) {
  return !t.isUnsigned || t.isInteger();
}

static bool isPointerCompatibleForAssign(const Type& dst, const Type& src) {
  if (!dst.isPointer() || !src.isPointer()) return false;
  Type d = dst;
  Type s = src;
  d.clearTopLevelConst();
  s.clearTopLevelConst();
  if (d == s) return true;
  if (d.isFunctionPointer() || s.isFunctionPointer()) return false;
  if (d.ptrDepth == 1 && s.ptrDepth == 1) {
    if (d.base == Type::Base::Void || s.base == Type::Base::Void) return true;
    if (d.base != s.base) return false;
    if (s.isConst && !d.isConst) return false;
    return true;
  }
  return false;
}

static Type adjustParamType(const Type& t) {
  if (!t.isArray()) return t;
  return t.decayType();
}

static Type stripAllQuals(const Type& t) {
  Type out = t;
  out.isConst = false;
  for (size_t i = 0; i < out.ptrConst.size(); ++i) out.ptrConst[i] = false;
  return out;
}

static bool samePointerTypeIgnoreQuals(const Type& lhs, const Type& rhs) {
  if (!lhs.isPointer() || !rhs.isPointer()) return false;
  Type l = stripAllQuals(lhs);
  Type r = stripAllQuals(rhs);
  return l == r;
}

static bool isArrayElementVoid(const Type& t) {
  return t.isArray() && t.base == Type::Base::Void && t.ptrDepth == 0;
}

static bool hasInvalidArraySize(const Type& t, bool allowFirstEmpty) {
  if (!t.isArray()) return false;
  for (size_t i = 0; i < t.arrayDims.size(); ++i) {
    const auto& dim = t.arrayDims[i];
    if (!dim.has_value()) {
      if (allowFirstEmpty && i == 0) continue;
      return true;
    }
    if (*dim == 0) return true;
  }
  return false;
}

static bool fillArraySizeFromString(DeclItem& item, Diagnostics& diags) {
  if (!item.type.isArray() || item.type.arrayDims.empty()) return true;
  if (item.type.arrayDims[0].has_value()) return true;
  if (!item.initExpr) {
    diags.error(item.nameLoc, "invalid array size");
    return false;
  }
  auto* str = dynamic_cast<StringLiteralExpr*>(item.initExpr.get());
  if (!str) return true;
  Type elem = item.type.elementType();
  if (elem.base != Type::Base::Char || elem.ptrDepth != 0 || !elem.arrayDims.empty()) {
    diags.error(item.nameLoc, "invalid array size");
    return false;
  }
  item.type.arrayDims[0] = str->value.size() + 1;
  return true;
}

static bool fillArraySizeFromInitList(DeclItem& item, Diagnostics& diags) {
  if (!item.type.isArray() || item.type.arrayDims.empty()) return true;
  if (item.type.arrayDims[0].has_value()) return true;
  if (!item.initExpr) {
    diags.error(item.nameLoc, "invalid array size");
    return false;
  }
  if (dynamic_cast<StringLiteralExpr*>(item.initExpr.get())) return true;
  auto* list = dynamic_cast<InitListExpr*>(item.initExpr.get());
  if (!list) {
    diags.error(item.nameLoc, "invalid array size");
    return false;
  }
  if (list->elems.size() == 1 && list->elems[0].designators.empty()) {
    if (auto* str = dynamic_cast<StringLiteralExpr*>(list->elems[0].expr.get())) {
      item.type.arrayDims[0] = str->value.size() + 1;
      return true;
    }
  }
  size_t next = 0;
  size_t max = 0;
  for (const auto& elem : list->elems) {
    if (elem.designators.empty()) {
      size_t idx = next;
      next = idx + 1;
      if (idx + 1 > max) max = idx + 1;
      continue;
    }
    if (elem.designators[0].kind != Designator::Kind::Index) {
      diags.error(item.nameLoc, "invalid array size");
      return false;
    }
    size_t idx = elem.designators[0].index;
    next = idx + 1;
    if (idx + 1 > max) max = idx + 1;
  }
  item.type.arrayDims[0] = max;
  return true;
}

static bool requiresStructDef(const Type& t) {
  return t.base == Type::Base::Struct && t.ptrDepth == 0;
}

static bool requiresEnumDef(const Type& t) {
  return t.base == Type::Base::Enum && !t.enumName.empty();
}

static bool checkInitializer(
    Diagnostics& diags,
    ScopeStack& scopes,
    const FnTable& fns,
    const StructTable& structs,
    const EnumConstTable& enums,
    const Type& target,
    Expr& init,
    bool allowArrayInit);

static bool checkInitList(
    Diagnostics& diags,
    ScopeStack& scopes,
    const FnTable& fns,
    const StructTable& structs,
    const EnumConstTable& enums,
    const Type& target,
    InitListExpr& list,
    bool allowArrayInit) {
  if (target.isArray() && !target.ptrOutsideArrays) {
    if (!allowArrayInit) {
      diags.error(list.loc, "array initializer not supported");
      return false;
    }
    if (target.arrayDims.empty() || !target.arrayDims[0].has_value()) {
      diags.error(list.loc, "invalid array initializer");
      return false;
    }
    size_t size = *target.arrayDims[0];
    Type elemTy = target.elementType();
    if (elemTy.base == Type::Base::Char && elemTy.ptrDepth == 0 &&
        elemTy.arrayDims.empty() && list.elems.size() == 1 &&
        list.elems[0].designators.empty()) {
      if (auto* str = asStringLiteral(*list.elems[0].expr)) {
        size_t need = str->value.size() + 1;
        if (size < need) {
          diags.error(list.loc, "string initializer too long");
          return false;
        }
        return true;
      }
    }
    size_t nextIndex = 0;
    for (const auto& elem : list.elems) {
      size_t idx = nextIndex;
      Type targetTy = elemTy;
      if (!elem.designators.empty()) {
        const auto& first = elem.designators[0];
        if (first.kind != Designator::Kind::Index) {
          diags.error(first.loc, "invalid array designator");
          return false;
        }
        idx = first.index;
        nextIndex = idx + 1;
        targetTy = target;
        for (const auto& d : elem.designators) {
          if (d.kind == Designator::Kind::Index) {
            if (!targetTy.isArray() || targetTy.ptrOutsideArrays) {
              diags.error(d.loc, "invalid array designator");
              return false;
            }
            if (targetTy.arrayDims.empty() || !targetTy.arrayDims[0].has_value()) {
              diags.error(d.loc, "invalid array initializer");
              return false;
            }
            size_t arrSize = *targetTy.arrayDims[0];
            if (d.index >= arrSize) {
              diags.error(d.loc, "array designator out of range");
              return false;
            }
            targetTy = targetTy.elementType();
          } else {
            if (targetTy.base != Type::Base::Struct || targetTy.ptrDepth != 0) {
              diags.error(d.loc, "invalid struct designator");
              return false;
            }
            const StructInfo* info = lookupStruct(structs, targetTy.structName);
            if (!info) {
              diags.error(d.loc, "unknown struct type '" + targetTy.structName + "'");
              return false;
            }
            bool found = false;
            for (const auto& field : info->fields) {
              if (field.name == d.field) {
                targetTy = field.type;
                found = true;
                break;
              }
            }
            if (!found) {
              diags.error(d.loc,
                          "unknown field '" + d.field + "' in struct '" + targetTy.structName + "'");
              return false;
            }
          }
        }
      } else {
        nextIndex = idx + 1;
      }
      if (idx >= size) {
        diags.error(list.loc, "excess elements in array initializer");
        return false;
      }
      if (!checkInitializer(diags, scopes, fns, structs, enums, targetTy, *elem.expr, true)) {
        return false;
      }
    }
    return true;
  }

  if (target.base == Type::Base::Struct && target.ptrDepth == 0) {
    const StructInfo* info = lookupStruct(structs, target.structName);
    if (!info) {
      diags.error(list.loc, "unknown struct type '" + target.structName + "'");
      return false;
    }
    size_t nextField = 0;
    for (const auto& elem : list.elems) {
      size_t idx = nextField;
      Type targetTy;
      if (!elem.designators.empty()) {
        const auto& first = elem.designators[0];
        if (first.kind != Designator::Kind::Field) {
          diags.error(first.loc, "invalid struct designator");
          return false;
        }
        bool found = false;
        for (size_t fi = 0; fi < info->fields.size(); ++fi) {
          if (info->fields[fi].name == first.field) {
            idx = fi;
            found = true;
            break;
          }
        }
        if (!found) {
          diags.error(first.loc,
                      "unknown field '" + first.field + "' in struct '" + target.structName + "'");
          return false;
        }
        nextField = idx + 1;
        targetTy = target;
        for (const auto& d : elem.designators) {
          if (d.kind == Designator::Kind::Field) {
            if (targetTy.base != Type::Base::Struct || targetTy.ptrDepth != 0) {
              diags.error(d.loc, "invalid struct designator");
              return false;
            }
            const StructInfo* curInfo = lookupStruct(structs, targetTy.structName);
            if (!curInfo) {
              diags.error(d.loc, "unknown struct type '" + targetTy.structName + "'");
              return false;
            }
            bool fieldFound = false;
            for (const auto& field : curInfo->fields) {
              if (field.name == d.field) {
                targetTy = field.type;
                fieldFound = true;
                break;
              }
            }
            if (!fieldFound) {
              diags.error(d.loc,
                          "unknown field '" + d.field + "' in struct '" + targetTy.structName + "'");
              return false;
            }
          } else {
            if (!targetTy.isArray() || targetTy.ptrOutsideArrays) {
              diags.error(d.loc, "invalid array designator");
              return false;
            }
            if (targetTy.arrayDims.empty() || !targetTy.arrayDims[0].has_value()) {
              diags.error(d.loc, "invalid array initializer");
              return false;
            }
            size_t arrSize = *targetTy.arrayDims[0];
            if (d.index >= arrSize) {
              diags.error(d.loc, "array designator out of range");
              return false;
            }
            targetTy = targetTy.elementType();
          }
        }
      } else {
        nextField = idx + 1;
        if (idx >= info->fields.size()) {
          diags.error(list.loc, "excess elements in struct initializer");
          return false;
        }
        targetTy = info->fields[idx].type;
      }
      if (idx >= info->fields.size()) {
        diags.error(list.loc, "excess elements in struct initializer");
        return false;
      }
      if (!checkInitializer(diags, scopes, fns, structs, enums, targetTy, *elem.expr, true)) {
        return false;
      }
    }
    return true;
  }

  if (list.elems.empty()) return true;
  if (list.elems.size() != 1 || !list.elems[0].designators.empty()) {
    diags.error(list.loc, "invalid initializer");
    return false;
  }
  return checkInitializer(diags, scopes, fns, structs, enums,
                          target, *list.elems[0].expr, allowArrayInit);
}

static bool checkInitializer(
    Diagnostics& diags,
    ScopeStack& scopes,
    const FnTable& fns,
    const StructTable& structs,
    const EnumConstTable& enums,
    const Type& target,
    Expr& init,
    bool allowArrayInit) {
  if (auto* list = dynamic_cast<InitListExpr*>(&init)) {
    return checkInitList(diags, scopes, fns, structs, enums, target, *list, allowArrayInit);
  }

  if (target.isArray() && !target.ptrOutsideArrays) {
    if (auto* str = asStringLiteral(init)) {
      Type elem = target.elementType();
      if (elem.base != Type::Base::Char || elem.ptrDepth != 0 || !elem.arrayDims.empty()) {
        diags.error(init.loc, "invalid string initializer");
        return false;
      }
      if (!target.arrayDims.empty() && target.arrayDims[0].has_value()) {
        size_t need = str->value.size() + 1;
        if (*target.arrayDims[0] < need) {
          diags.error(init.loc, "string initializer too long");
          return false;
        }
      }
      return true;
    }
    if (allowArrayInit) {
      diags.error(init.loc, "invalid initializer for array");
    } else {
      diags.error(init.loc, "array initializer not supported");
    }
    return false;
  }

  auto initTy = checkExprImpl(diags, scopes, fns, structs, enums, init);
  if (initTy && !isAssignable(target, *initTy, init)) {
    diags.error(init.loc, "incompatible initializer");
    return false;
  }
  return true;
}

static std::optional<Type> resolveMemberType(
    Diagnostics& diags,
    const StructTable& structs,
    const Type& baseTy,
    const std::string& member,
    SourceLocation memberLoc,
    bool isArrow) {
  Type structTy = baseTy;
  if (isArrow) {
    if (!baseTy.isPointer() || baseTy.ptrDepth != 1 || baseTy.base != Type::Base::Struct) {
      diags.error(memberLoc, "member access requires pointer to struct");
      return std::nullopt;
    }
    structTy = baseTy.pointee();
  } else {
    if (baseTy.base != Type::Base::Struct || baseTy.ptrDepth != 0) {
      diags.error(memberLoc, "member access requires struct");
      return std::nullopt;
    }
  }

  const StructInfo* info = lookupStruct(structs, structTy.structName);
  if (!info) {
    diags.error(memberLoc, "unknown struct type '" + structTy.structName + "'");
    return std::nullopt;
  }

  for (const auto& field : info->fields) {
    if (field.name == member) return field.type;
  }

  diags.error(memberLoc,
              "unknown field '" + member + "' in struct '" + structTy.structName + "'");
  return std::nullopt;
}

static void checkStmtImpl(
    Diagnostics& diags,
    ScopeStack& scopes,
    const FnTable& fns,
    const StructTable& structs,
    const EnumConstTable& enums,
    const EnumTypeTable& enumTypes,
    const Type& returnType,
    int loopDepth,
    int switchDepth,
    Stmt& s) {
  if (auto* blk = dynamic_cast<BlockStmt*>(&s)) {
    scopes.push_back({});
    for (const auto& st : blk->stmts) {
      checkStmtImpl(diags, scopes, fns, structs, enums, enumTypes,
                    returnType, loopDepth, switchDepth, *st);
    }
    scopes.pop_back();
    return;
  }

  if (auto* iff = dynamic_cast<IfStmt*>(&s)) {
    checkExprImpl(diags, scopes, fns, structs, enums, *iff->cond);
    checkStmtImpl(diags, scopes, fns, structs, enums, enumTypes,
                  returnType, loopDepth, switchDepth, *iff->thenBranch);
    if (iff->elseBranch) {
      checkStmtImpl(diags, scopes, fns, structs, enums, enumTypes,
                    returnType, loopDepth, switchDepth, *iff->elseBranch);
    }
    return;
  }

  if (auto* wh = dynamic_cast<WhileStmt*>(&s)) {
    checkExprImpl(diags, scopes, fns, structs, enums, *wh->cond);
    checkStmtImpl(diags, scopes, fns, structs, enums, enumTypes,
                  returnType, loopDepth + 1, switchDepth, *wh->body);
    return;
  }

  if (auto* dw = dynamic_cast<DoWhileStmt*>(&s)) {
    checkStmtImpl(diags, scopes, fns, structs, enums, enumTypes,
                  returnType, loopDepth + 1, switchDepth, *dw->body);
    checkExprImpl(diags, scopes, fns, structs, enums, *dw->cond);
    return;
  }

  if (auto* fo = dynamic_cast<ForStmt*>(&s)) {
    // for introduces its own scope (matches your existing tests)
    scopes.push_back({});
    if (fo->init) checkStmtImpl(diags, scopes, fns, structs, enums, enumTypes,
                                returnType, loopDepth, switchDepth, *fo->init);
    if (fo->cond) checkExprImpl(diags, scopes, fns, structs, enums, *fo->cond);
    if (fo->inc)  checkExprImpl(diags, scopes, fns, structs, enums, *fo->inc);
    checkStmtImpl(diags, scopes, fns, structs, enums, enumTypes,
                  returnType, loopDepth + 1, switchDepth, *fo->body);
    scopes.pop_back();
    return;
  }

  if (auto* br = dynamic_cast<BreakStmt*>(&s)) {
    if (loopDepth <= 0 && switchDepth <= 0) diags.error(br->loc, "break statement not within loop");
    return;
  }

  if (auto* co = dynamic_cast<ContinueStmt*>(&s)) {
    if (loopDepth <= 0) diags.error(co->loc, "continue statement not within loop");
    return;
  }

  if (auto* sw = dynamic_cast<SwitchStmt*>(&s)) {
    auto condTy = checkExprImpl(diags, scopes, fns, structs, enums, *sw->cond);
    if (condTy && !condTy->isInteger()) {
      diags.error(sw->cond->loc, "switch condition must be int");
    }
    scopes.push_back({});
    std::unordered_set<int64_t> seenCases;
    bool seenDefault = false;
    for (const auto& c : sw->cases) {
      if (c.value.has_value()) {
        int64_t v = *c.value;
        if (seenCases.count(v)) {
          diags.error(c.loc, "duplicate case value '" + std::to_string(v) + "'");
          scopes.pop_back();
          return;
        }
        seenCases.insert(v);
      } else {
        if (seenDefault) {
          diags.error(c.loc, "duplicate default label");
          scopes.pop_back();
          return;
        }
        seenDefault = true;
      }
      for (const auto& st : c.stmts) {
        checkStmtImpl(diags, scopes, fns, structs, enums, enumTypes,
                      returnType, loopDepth, switchDepth + 1, *st);
      }
    }
    scopes.pop_back();
    return;
  }

  if (auto* decl = dynamic_cast<DeclStmt*>(&s)) {
    auto& cur = scopes.back();
    for (auto& item : decl->items) {
      if (cur.count(item.name)) {
        diags.error(item.nameLoc, "redefinition of '" + item.name + "'");
        return;
      }
      if (!isValidUnsignedUse(item.type)) {
        diags.error(item.nameLoc, "invalid use of unsigned type");
        return;
      }
      if (isArrayElementVoid(item.type)) {
        diags.error(item.nameLoc, "invalid array element type");
        return;
      }
      if (item.type.isVoidObject()) {
        diags.error(item.nameLoc, "invalid use of void type");
        return;
      }
      if (!fillArraySizeFromString(item, diags)) {
        return;
      }
      if (!fillArraySizeFromInitList(item, diags)) {
        return;
      }
      if (hasInvalidArraySize(item.type, /*allowFirstEmpty=*/false)) {
        diags.error(item.nameLoc, "invalid array size");
        return;
      }
      if (requiresStructDef(item.type)) {
        if (!lookupStruct(structs, item.type.structName)) {
          diags.error(item.nameLoc, "unknown struct type '" + item.type.structName + "'");
          return;
        }
      }
      if (requiresEnumDef(item.type)) {
        if (!enumTypes.count(item.type.enumName)) {
          diags.error(item.nameLoc, "unknown enum type '" + item.type.enumName + "'");
          return;
        }
      }

      // initializer cannot reference the variable being declared:
      // keep behavior by checking before insertion.
      if (item.initExpr) {
        bool allowArrayInit = item.type.isArray() && !item.type.ptrOutsideArrays;
        if (!checkInitializer(diags, scopes, fns, structs, enums,
                              item.type, *item.initExpr, allowArrayInit)) {
          return;
        }
      }

      cur.emplace(item.name, item.type);
    }
    return;
  }

  if (auto* as = dynamic_cast<AssignStmt*>(&s)) {
    // legacy stmt (if still exists somewhere)
    checkExprImpl(diags, scopes, fns, structs, enums, *as->valueExpr);
    if (!lookupVarType(scopes, as->name).has_value()) {
      diags.error(as->nameLoc, "assignment to undeclared identifier '" + as->name + "'");
    }
    return;
  }

  if (auto* ret = dynamic_cast<ReturnStmt*>(&s)) {
    if (!ret->valueExpr) {
      if (!returnType.isVoidObject()) {
        diags.error(ret->loc, "missing return value");
      }
      return;
    }
    auto retTy = checkExprImpl(diags, scopes, fns, structs, enums, *ret->valueExpr);
    if (retTy && !isAssignable(returnType, *retTy, *ret->valueExpr)) {
      diags.error(ret->loc, "incompatible return type");
    }
    return;
  }

  if (auto* es = dynamic_cast<ExprStmt*>(&s)) {
    checkExprImpl(diags, scopes, fns, structs, enums, *es->expr);
    return;
  }

  if (auto* td = dynamic_cast<TypedefStmt*>(&s)) {
    (void)td;
    return;
  }

  if (dynamic_cast<EmptyStmt*>(&s)) return;
}

static std::optional<Type> checkLValue(
    Diagnostics& diags,
    ScopeStack& scopes,
    const FnTable& fns,
    const StructTable& structs,
    const EnumConstTable& enums,
    Expr& e,
    const char* errMsg,
    bool isAssign) {
  if (auto* vr = dynamic_cast<VarRefExpr*>(&e)) {
    auto ty = lookupVarType(scopes, vr->name);
    if (!ty) {
      if (isAssign) {
        diags.error(vr->loc, "assignment to undeclared identifier '" + vr->name + "'");
      } else {
        diags.error(vr->loc, "use of undeclared identifier '" + vr->name + "'");
      }
      return std::nullopt;
    }
    if (ty->isArray() && !ty->ptrOutsideArrays) {
      if (isAssign) {
        diags.error(vr->loc, "cannot assign to array");
        return std::nullopt;
      }
      diags.error(vr->loc, "cannot take address of array");
      return std::nullopt;
    }
    if (isAssign && ty->isTopLevelConst()) {
      diags.error(vr->loc, "cannot assign to const object");
      return std::nullopt;
    }
    e.semaType = *ty;
    return *ty;
  }

  if (auto* un = dynamic_cast<UnaryExpr*>(&e)) {
    if (un->op == TokenKind::Star) {
      auto opTy = checkExprImpl(diags, scopes, fns, structs, enums, *un->operand);
      if (!opTy) return std::nullopt;
      if (!opTy->isPointer()) {
        diags.error(un->loc, "cannot dereference non-pointer");
        return std::nullopt;
      }
      Type t = opTy->pointee();
      if (isAssign && t.isTopLevelConst()) {
        diags.error(un->loc, "cannot assign to const object");
        return std::nullopt;
      }
      e.semaType = t;
      return t;
    }
  }

  if (auto* sub = dynamic_cast<SubscriptExpr*>(&e)) {
    auto baseTy = checkExprImpl(diags, scopes, fns, structs, enums, *sub->base);
    auto idxTy = checkExprImpl(diags, scopes, fns, structs, enums, *sub->index);
    if (!baseTy || !idxTy) return std::nullopt;
    if (baseTy->isArray() && !baseTy->ptrOutsideArrays) {
      Type dt = baseTy->decayType();
      baseTy = dt;
    }
    if (!idxTy->isInteger()) {
      diags.error(sub->index->loc, "array subscript must be int");
      return std::nullopt;
    }
    if (baseTy->isPointer() && baseTy->isVoidPointer()) {
      diags.error(sub->base->loc, "cannot subscript void pointer");
      return std::nullopt;
    }
    if (!baseTy->isPointer()) {
      diags.error(sub->base->loc, "subscripted value is not pointer");
      return std::nullopt;
    }
    Type elem = baseTy->pointee();
    if (isAssign && elem.isTopLevelConst()) {
      diags.error(sub->loc, "cannot assign to const object");
      return std::nullopt;
    }
    e.semaType = elem;
    return elem;
  }

  if (auto* mem = dynamic_cast<MemberExpr*>(&e)) {
    auto baseTy = checkExprImpl(diags, scopes, fns, structs, enums, *mem->base);
    if (!baseTy) return std::nullopt;
    auto fieldTy = resolveMemberType(
        diags, structs, *baseTy, mem->member, mem->memberLoc, mem->isArrow);
    if (!fieldTy) return std::nullopt;
    if (fieldTy->isArray() && !fieldTy->ptrOutsideArrays) {
      if (isAssign) {
        diags.error(mem->memberLoc, "cannot assign to array");
      } else {
        diags.error(mem->memberLoc, "cannot take address of array");
      }
      return std::nullopt;
    }
    if (isAssign && fieldTy->isTopLevelConst()) {
      diags.error(mem->memberLoc, "cannot assign to const object");
      return std::nullopt;
    }
    e.semaType = *fieldTy;
    return *fieldTy;
  }

  diags.error(e.loc, errMsg);
  return std::nullopt;
}

static bool isAssignable(const Type& dst, const Type& src, const Expr& srcExpr) {
  Type d = dst;
  Type s = src;
  d.clearTopLevelConst();
  s.clearTopLevelConst();
  if (d == s) return true;
  if (d.isNumeric() && s.isNumeric()) return true;
  if (dst.isPointer() && src.isInt() && isNullPointerConstant(srcExpr)) return true;
  if (isPointerCompatibleForAssign(dst, src)) return true;
  return false;
}

static std::optional<Type> checkExprImpl(
    Diagnostics& diags, ScopeStack& scopes, const FnTable& fns, const StructTable& structs,
    const EnumConstTable& enums, Expr& e) {
  if (auto* lit = dynamic_cast<IntLiteralExpr*>(&e)) {
    Type t;
    e.semaType = t;
    return t;
  }
  if (auto* flt = dynamic_cast<FloatLiteralExpr*>(&e)) {
    Type t;
    t.base = flt->isFloat ? Type::Base::Float : Type::Base::Double;
    e.semaType = t;
    return t;
  }
  if (dynamic_cast<StringLiteralExpr*>(&e)) {
    Type t;
    t.base = Type::Base::Char;
    t.addPointerLevel(false);
    e.semaType = t;
    return t;
  }

  if (dynamic_cast<InitListExpr*>(&e)) {
    diags.error(e.loc, "initializer list not allowed here");
    return std::nullopt;
  }

  if (auto* inc = dynamic_cast<IncDecExpr*>(&e)) {
    auto lvTy = checkLValue(diags, scopes, fns, structs, enums, *inc->operand,
                            "expected lvalue for increment/decrement",
                            /*isAssign=*/true);
    if (!lvTy) return std::nullopt;
    if (lvTy->isPointer() && lvTy->isVoidPointer()) {
      diags.error(inc->loc, "invalid operand to ++/--");
      return std::nullopt;
    }
    if (!lvTy->isInteger() && !lvTy->isPointer()) {
      diags.error(inc->loc, "invalid operand to ++/--");
      return std::nullopt;
    }
    e.semaType = *lvTy;
    return *lvTy;
  }

  if (auto* sz = dynamic_cast<SizeofExpr*>(&e)) {
    if (sz->isType) {
      if (sz->type.isVoidObject()) {
        diags.error(sz->loc, "sizeof of void");
        return std::nullopt;
      }
    } else {
      if (auto* vr = dynamic_cast<VarRefExpr*>(sz->expr.get())) {
        auto ty = lookupVarType(scopes, vr->name);
        if (!ty) {
          diags.error(vr->loc, "use of undeclared identifier '" + vr->name + "'");
          return std::nullopt;
        }
        if (ty->isVoidObject()) {
          diags.error(sz->loc, "sizeof of void");
          return std::nullopt;
        }
        vr->semaType = *ty;
      } else {
        auto ty = checkExprImpl(diags, scopes, fns, structs, enums, *sz->expr);
        if (!ty) return std::nullopt;
        if (ty->isVoidObject()) {
          diags.error(sz->loc, "sizeof of void");
          return std::nullopt;
        }
      }
    }
    Type t;
    e.semaType = t;
    return t;
  }

  if (auto* cast = dynamic_cast<CastExpr*>(&e)) {
    auto opTy = checkExprImpl(diags, scopes, fns, structs, enums, *cast->expr);
    if (!opTy) return std::nullopt;
    if (cast->targetType.isArray()) {
      diags.error(cast->loc, "invalid cast target");
      return std::nullopt;
    }
    if (cast->targetType.isStruct()) {
      diags.error(cast->loc, "invalid cast target");
      return std::nullopt;
    }
    if (opTy->isVoidObject()) {
      diags.error(cast->loc, "invalid cast from void");
      return std::nullopt;
    }
    bool ok = false;
    if (cast->targetType.isVoidObject()) {
      ok = true;
    } else if (cast->targetType.isPointer()) {
      ok = opTy->isPointer() || opTy->isInteger();
    } else if (cast->targetType.isInteger()) {
      ok = opTy->isNumeric() || opTy->isPointer();
    } else if (cast->targetType.isFloating()) {
      ok = opTy->isNumeric();
    }
    if (!ok) {
      diags.error(cast->loc, "invalid cast");
      return std::nullopt;
    }
    e.semaType = cast->targetType;
    return cast->targetType;
  }

  if (auto* vr = dynamic_cast<VarRefExpr*>(&e)) {
    auto ty = lookupVarType(scopes, vr->name);
    if (!ty) {
      auto it = enums.find(vr->name);
      if (it != enums.end()) {
        Type t;
        e.semaType = t;
        return t;
      }
      auto fit = fns.find(vr->name);
      if (fit != fns.end()) {
        Type t = functionPointerTypeFromFnInfo(fit->second);
        e.semaType = t;
        return t;
      }
      diags.error(vr->loc, "use of undeclared identifier '" + vr->name + "'");
      return std::nullopt;
    }
    if (ty->isArray() && !ty->ptrOutsideArrays) {
      Type dt = ty->decayType();
      e.semaType = dt;
      return dt;
    }
    e.semaType = *ty;
    return *ty;
  }

  if (auto* call = dynamic_cast<CallExpr*>(&e)) {
    const FunctionType* fnTy = nullptr;
    FunctionType fnFromInfo;
    SourceLocation calleeLoc = call->calleeLoc;
    if (call->calleeExpr) {
      calleeLoc = call->calleeExpr->loc;
      auto calleeTy = checkExprImpl(diags, scopes, fns, structs, enums, *call->calleeExpr);
      if (!calleeTy) return std::nullopt;
      if (!calleeTy->func || calleeTy->ptrDepth > 1) {
        diags.error(calleeLoc, "called object is not a function");
        for (const auto& a : call->args) checkExprImpl(diags, scopes, fns, structs, enums, *a);
        return std::nullopt;
      }
      fnTy = calleeTy->func.get();
    } else {
      auto varTy = lookupVarType(scopes, call->callee);
      if (varTy) {
        if (!varTy->func || varTy->ptrDepth != 1) {
          diags.error(calleeLoc, "called object is not a function");
          for (const auto& a : call->args) checkExprImpl(diags, scopes, fns, structs, enums, *a);
          return std::nullopt;
        }
        fnTy = varTy->func.get();
      } else {
        auto it = fns.find(call->callee);
        if (it == fns.end()) {
          diags.error(calleeLoc, "call to undeclared function '" + call->callee + "'");
          for (const auto& a : call->args) checkExprImpl(diags, scopes, fns, structs, enums, *a);
          return std::nullopt;
        }
        fnFromInfo.returnType = it->second.returnType;
        fnFromInfo.params = it->second.paramTypes;
        fnFromInfo.isVariadic = it->second.isVariadic;
        fnTy = &fnFromInfo;
      }
    }

    size_t expected = fnTy->params.size();
    size_t have = call->args.size();
    if (fnTy->isVariadic) {
      if (have < expected) {
        diags.error(calleeLoc,
                    "expected at least " + std::to_string(expected) +
                        " arguments, have " + std::to_string(have));
      }
    } else if (expected != have) {
      diags.error(calleeLoc,
                  "expected " + std::to_string(expected) +
                      " arguments, have " + std::to_string(have));
    }

    for (size_t i = 0; i < call->args.size(); ++i) {
      auto argTy = checkExprImpl(diags, scopes, fns, structs, enums, *call->args[i]);
      if (!argTy || i >= fnTy->params.size()) continue;
      if (!isAssignable(fnTy->params[i], *argTy, *call->args[i])) {
        diags.error(call->args[i]->loc, "incompatible argument type");
      }
    }

    e.semaType = fnTy->returnType;
    return fnTy->returnType;
  }

  if (auto* asn = dynamic_cast<AssignExpr*>(&e)) {
    auto lhsTy = checkLValue(diags, scopes, fns, structs, enums, *asn->lhs,
                             "expected lvalue on left-hand side of assignment",
                             /*isAssign=*/true);
    auto rhsTy = checkExprImpl(diags, scopes, fns, structs, enums, *asn->rhs);
    if (!lhsTy || !rhsTy) return std::nullopt;
    if (asn->op == TokenKind::Assign) {
      if (!isAssignable(*lhsTy, *rhsTy, *asn->rhs)) {
        diags.error(asn->loc, "incompatible assignment");
      }
      e.semaType = *lhsTy;
      return *lhsTy;
    }

    auto report = [&](const std::string& msg) -> std::optional<Type> {
      diags.error(asn->loc, msg);
      return std::nullopt;
    };

    switch (asn->op) {
      case TokenKind::PlusAssign:
      case TokenKind::MinusAssign: {
        if (lhsTy->isNumeric() && rhsTy->isNumeric()) {
          e.semaType = *lhsTy;
          return *lhsTy;
        }
        if (lhsTy->isPointer() && rhsTy->isInteger() && !lhsTy->isVoidPointer()) {
          e.semaType = *lhsTy;
          return *lhsTy;
        }
        return report("invalid operands to pointer arithmetic");
      }
      case TokenKind::StarAssign:
      case TokenKind::SlashAssign: {
        if (!lhsTy->isNumeric() || !rhsTy->isNumeric()) {
          return report("invalid operands to compound assignment");
        }
        e.semaType = *lhsTy;
        return *lhsTy;
      }
      case TokenKind::PercentAssign: {
        if (!lhsTy->isInteger() || !rhsTy->isInteger()) {
          return report("invalid operands to compound assignment");
        }
        e.semaType = *lhsTy;
        return *lhsTy;
      }
      case TokenKind::LessLessAssign:
      case TokenKind::GreaterGreaterAssign: {
        if (!lhsTy->isInteger() || !rhsTy->isInteger()) {
          return report("invalid operands to shift operator");
        }
        e.semaType = *lhsTy;
        return *lhsTy;
      }
      case TokenKind::AmpAssign:
      case TokenKind::PipeAssign:
      case TokenKind::CaretAssign: {
        if (!lhsTy->isInteger() || !rhsTy->isInteger()) {
          return report("invalid operands to bitwise operator");
        }
        e.semaType = *lhsTy;
        return *lhsTy;
      }
      default:
        return report("invalid operands to compound assignment");
    }
  }

  if (auto* ter = dynamic_cast<TernaryExpr*>(&e)) {
    auto condTy = checkExprImpl(diags, scopes, fns, structs, enums, *ter->cond);
    auto thenTy = checkExprImpl(diags, scopes, fns, structs, enums, *ter->thenExpr);
    auto elseTy = checkExprImpl(diags, scopes, fns, structs, enums, *ter->elseExpr);
    if (condTy && !isScalarType(*condTy)) {
      diags.error(ter->cond->loc, "condition must be scalar");
    }
    if (!thenTy || !elseTy) return std::nullopt;
    if (*thenTy == *elseTy) {
      e.semaType = *thenTy;
      return *thenTy;
    }
    if (thenTy->isNumeric() && elseTy->isNumeric()) {
      Type t = commonNumericType(*thenTy, *elseTy);
      e.semaType = t;
      return t;
    }
    if (thenTy->isPointer() && elseTy->isInt() && isNullPointerConstant(*ter->elseExpr)) {
      e.semaType = *thenTy;
      return *thenTy;
    }
    if (elseTy->isPointer() && thenTy->isInt() && isNullPointerConstant(*ter->thenExpr)) {
      e.semaType = *elseTy;
      return *elseTy;
    }
    diags.error(ter->loc, "incompatible types in conditional operator");
    return std::nullopt;
  }

  if (auto* un = dynamic_cast<UnaryExpr*>(&e)) {
    if (un->op == TokenKind::Amp) {
      auto lvTy = checkLValue(diags, scopes, fns, structs, enums, *un->operand,
                              "expected lvalue for address-of operator",
                              /*isAssign=*/false);
      if (!lvTy) return std::nullopt;
      Type t = *lvTy;
      t.addPointerLevel(false);
      t.ptrOutsideArrays = false;
      e.semaType = t;
      return t;
    }

    auto opTy = checkExprImpl(diags, scopes, fns, structs, enums, *un->operand);
    if (!opTy) return std::nullopt;

    if (un->op == TokenKind::Star) {
      if (!opTy->isPointer()) {
        diags.error(un->loc, "cannot dereference non-pointer");
        return std::nullopt;
      }
      if (opTy->isVoidPointer()) {
        diags.error(un->loc, "cannot dereference void pointer");
        return std::nullopt;
      }
      Type t = opTy->pointee();
      e.semaType = t;
      return t;
    }

    if (un->op == TokenKind::Bang) {
      if (!isScalarType(*opTy)) {
        diags.error(un->loc, "invalid operand to '!'");
        return std::nullopt;
      }
      Type t;
      e.semaType = t;
      return t;
    }

    if (un->op == TokenKind::Plus || un->op == TokenKind::Minus || un->op == TokenKind::Tilde) {
      if (un->op == TokenKind::Tilde && !opTy->isInteger()) {
        diags.error(un->loc, "invalid operand to unary operator");
        return std::nullopt;
      }
      if (un->op != TokenKind::Tilde && !opTy->isNumeric()) {
        diags.error(un->loc, "invalid operand to unary operator");
        return std::nullopt;
      }
      Type t = opTy->isFloating() ? *opTy : promoteInteger(*opTy);
      e.semaType = t;
      return t;
    }
  }

  if (auto* bin = dynamic_cast<BinaryExpr*>(&e)) {
    auto lhsTy = checkExprImpl(diags, scopes, fns, structs, enums, *bin->lhs);
    auto rhsTy = checkExprImpl(diags, scopes, fns, structs, enums, *bin->rhs);
    if (!lhsTy || !rhsTy) return std::nullopt;

    switch (bin->op) {
      case TokenKind::Comma: {
        e.semaType = *rhsTy;
        return *rhsTy;
      }
      case TokenKind::AmpAmp:
      case TokenKind::PipePipe: {
        if (!isScalarType(*lhsTy) || !isScalarType(*rhsTy)) {
          diags.error(bin->loc, "invalid operands to logical operator");
          return std::nullopt;
        }
        Type t;
        e.semaType = t;
        return t;
      }
      case TokenKind::EqualEqual:
      case TokenKind::BangEqual: {
        if (*lhsTy == *rhsTy || samePointerTypeIgnoreQuals(*lhsTy, *rhsTy)) {
          Type t;
          e.semaType = t;
          return t;
        }
        if (lhsTy->isNumeric() && rhsTy->isNumeric()) {
          Type t;
          e.semaType = t;
          return t;
        }
        if (lhsTy->isPointer() && rhsTy->isPointer() &&
            lhsTy->ptrDepth == 1 && rhsTy->ptrDepth == 1 &&
            (lhsTy->base == Type::Base::Void || rhsTy->base == Type::Base::Void)) {
          Type t;
          e.semaType = t;
          return t;
        }
        if (lhsTy->isPointer() && rhsTy->isInt() && isNullPointerConstant(*bin->rhs)) {
          Type t;
          e.semaType = t;
          return t;
        }
        if (rhsTy->isPointer() && lhsTy->isInt() && isNullPointerConstant(*bin->lhs)) {
          Type t;
          e.semaType = t;
          return t;
        }
        diags.error(bin->loc, "invalid operands to equality operator");
        return std::nullopt;
      }
      case TokenKind::Less:
      case TokenKind::LessEqual:
      case TokenKind::Greater:
      case TokenKind::GreaterEqual: {
        if (!(lhsTy->isNumeric() && rhsTy->isNumeric())) {
          if (!(lhsTy->isPointer() && rhsTy->isPointer() &&
                samePointerTypeIgnoreQuals(*lhsTy, *rhsTy) && !lhsTy->isVoidPointer())) {
            diags.error(bin->loc, "invalid operands to relational operator");
            return std::nullopt;
          }
        }
        Type t;
        e.semaType = t;
        return t;
      }
      case TokenKind::Plus:
      case TokenKind::Minus: {
        if (lhsTy->isNumeric() && rhsTy->isNumeric()) {
          Type t = commonNumericType(*lhsTy, *rhsTy);
          e.semaType = t;
          return t;
        }
        if (lhsTy->isPointer() && rhsTy->isInteger() && !lhsTy->isVoidPointer()) {
          e.semaType = *lhsTy;
          return *lhsTy;
        }
        if (bin->op == TokenKind::Plus && lhsTy->isInteger() && rhsTy->isPointer() &&
            !rhsTy->isVoidPointer()) {
          e.semaType = *rhsTy;
          return *rhsTy;
        }
        if (bin->op == TokenKind::Minus && lhsTy->isPointer() && rhsTy->isPointer() &&
            samePointerTypeIgnoreQuals(*lhsTy, *rhsTy) && !lhsTy->isVoidPointer()) {
          Type t;
          e.semaType = t;
          return t;
        }
        diags.error(bin->loc, "invalid operands to pointer arithmetic");
        return std::nullopt;
      }
      case TokenKind::Star:
      case TokenKind::Slash: {
        if (!lhsTy->isNumeric() || !rhsTy->isNumeric()) {
          diags.error(bin->loc, "invalid operands to arithmetic operator");
          return std::nullopt;
        }
        Type t = commonNumericType(*lhsTy, *rhsTy);
        e.semaType = t;
        return t;
      }
      case TokenKind::Percent: {
        if (!lhsTy->isInteger() || !rhsTy->isInteger()) {
          diags.error(bin->loc, "invalid operands to arithmetic operator");
          return std::nullopt;
        }
        Type t = commonIntegerType(*lhsTy, *rhsTy);
        e.semaType = t;
        return t;
      }
      case TokenKind::LessLess:
      case TokenKind::GreaterGreater: {
        if (!lhsTy->isInteger() || !rhsTy->isInteger()) {
          diags.error(bin->loc, "invalid operands to shift operator");
          return std::nullopt;
        }
        Type t = promoteInteger(*lhsTy);
        e.semaType = t;
        return t;
      }
      case TokenKind::Amp:
      case TokenKind::Pipe:
      case TokenKind::Caret: {
        if (!lhsTy->isInteger() || !rhsTy->isInteger()) {
          diags.error(bin->loc, "invalid operands to bitwise operator");
          return std::nullopt;
        }
        Type t = commonIntegerType(*lhsTy, *rhsTy);
        e.semaType = t;
        return t;
      }
      default:
        break;
    }
  }

  if (auto* sub = dynamic_cast<SubscriptExpr*>(&e)) {
    auto baseTy = checkExprImpl(diags, scopes, fns, structs, enums, *sub->base);
    auto idxTy = checkExprImpl(diags, scopes, fns, structs, enums, *sub->index);
    if (!baseTy || !idxTy) return std::nullopt;
    if (baseTy->isArray() && !baseTy->ptrOutsideArrays) {
      Type dt = baseTy->decayType();
      baseTy = dt;
    }
    if (!idxTy->isInteger()) {
      diags.error(sub->index->loc, "array subscript must be int");
      return std::nullopt;
    }
    if (baseTy->isPointer() && baseTy->isVoidPointer()) {
      diags.error(sub->base->loc, "cannot subscript void pointer");
      return std::nullopt;
    }
    if (!baseTy->isPointer()) {
      diags.error(sub->base->loc, "subscripted value is not pointer");
      return std::nullopt;
    }
    Type elem = baseTy->pointee();
    e.semaType = elem;
    return elem;
  }

  if (auto* mem = dynamic_cast<MemberExpr*>(&e)) {
    auto baseTy = checkExprImpl(diags, scopes, fns, structs, enums, *mem->base);
    if (!baseTy) return std::nullopt;
    auto fieldTy = resolveMemberType(
        diags, structs, *baseTy, mem->member, mem->memberLoc, mem->isArrow);
    if (!fieldTy) return std::nullopt;
    if (fieldTy->isArray() && !fieldTy->ptrOutsideArrays) {
      Type dt = fieldTy->decayType();
      e.semaType = dt;
      return dt;
    }
    e.semaType = *fieldTy;
    return *fieldTy;
  }

  return std::nullopt;
}

static void addOrCheckFn(
    Diagnostics& diags,
    FnTable& fns,
    const FunctionProto& proto,
    bool isDef) {
  auto it = fns.find(proto.name);
  if (it == fns.end()) {
    FnInfo info;
    info.paramTypes.reserve(proto.params.size());
    for (const auto& prm : proto.params) info.paramTypes.push_back(adjustParamType(prm.type));
    info.returnType = proto.returnType;
    info.isVariadic = proto.isVariadic;
    info.isStatic = (proto.storage == StorageClass::Static);
    info.firstLoc = proto.nameLoc;
    info.hasDecl = !isDef;
    info.hasDef = isDef;
    fns.emplace(proto.name, info);
    return;
  }

  FnInfo& info = it->second;

  if (info.isStatic != (proto.storage == StorageClass::Static)) {
    diags.error(proto.nameLoc,
                "conflicting storage class for '" + proto.name + "'");
    return;
  }

  // signature mismatch
  if (!sameSignature(info, proto)) {
    diags.error(proto.nameLoc,
                "conflicting types for '" + proto.name + "'");
    return;
  }

  // same signature: update decl/def flags
  if (isDef) {
    if (info.hasDef) {
      diags.error(proto.nameLoc, "redefinition of '" + proto.name + "'");
      return;
    }
    info.hasDef = true;
  } else {
    info.hasDecl = true; // repeated decl ok
  }
}

} // namespace

bool Sema::run(AstTranslationUnit& tu) {
  // 0) collect struct definitions
  StructTable structs;
  EnumConstTable enumConsts;
  std::unordered_set<std::string> enumNames;
  for (const auto& item : tu.items) {
    auto* ed = std::get_if<EnumDef>(&item);
    if (ed) {
      if (ed->name.has_value()) {
        if (!enumNames.insert(*ed->name).second) {
          diags_.error(ed->nameLoc, "redefinition of 'enum " + *ed->name + "'");
          return false;
        }
      }
      for (const auto& it : ed->items) {
        if (!enumConsts.emplace(it.name, it.value).second) {
          diags_.error(it.nameLoc, "redefinition of enum constant '" + it.name + "'");
          return false;
        }
      }
    }
    auto* sd = std::get_if<StructDef>(&item);
    if (!sd) continue;
    if (structs.count(sd->name)) {
      diags_.error(sd->nameLoc, "redefinition of 'struct " + sd->name + "'");
      return false;
    }
    StructInfo info;
    info.fields = sd->fields;
    info.nameLoc = sd->nameLoc;
    structs.emplace(sd->name, std::move(info));
  }

  for (const auto& item : tu.items) {
    auto* sd = std::get_if<StructDef>(&item);
    if (!sd) continue;
    std::unordered_set<std::string> fieldNames;
    for (const auto& field : sd->fields) {
      if (!fieldNames.insert(field.name).second) {
        diags_.error(field.nameLoc, "duplicate field name '" + field.name + "'");
        return false;
      }
      if (!isValidUnsignedUse(field.type)) {
        diags_.error(field.nameLoc, "invalid field type");
        return false;
      }
      if (isArrayElementVoid(field.type)) {
        diags_.error(field.nameLoc, "invalid field type");
        return false;
      }
      if (field.type.isVoidObject()) {
        diags_.error(field.nameLoc, "invalid field type");
        return false;
      }
      if (hasInvalidArraySize(field.type, /*allowFirstEmpty=*/false)) {
        diags_.error(field.nameLoc, "invalid array size");
        return false;
      }
      if (requiresStructDef(field.type)) {
        if (field.type.structName == sd->name) {
          diags_.error(field.nameLoc, "field has incomplete type");
          return false;
        }
        if (!lookupStruct(structs, field.type.structName)) {
          diags_.error(field.nameLoc, "unknown struct type '" + field.type.structName + "'");
          return false;
        }
      }
      if (requiresEnumDef(field.type)) {
        if (!enumNames.count(field.type.enumName)) {
          diags_.error(field.nameLoc, "unknown enum type '" + field.type.enumName + "'");
          return false;
        }
      }
    }
  }

  // 1) collect all function prototypes (decls + defs)
  FnTable fns;
  for (const auto& item : tu.items) {
    if (auto* d = std::get_if<FunctionDecl>(&item)) {
      addOrCheckFn(diags_, fns, d->proto, /*isDef=*/false);
    } else if (auto* def = std::get_if<FunctionDef>(&item)) {
      addOrCheckFn(diags_, fns, def->proto, /*isDef=*/true);
    }
  }

  for (const auto& item : tu.items) {
    const FunctionProto* p = nullptr;
    if (auto* d = std::get_if<FunctionDecl>(&item)) p = &d->proto;
    else if (auto* def = std::get_if<FunctionDef>(&item)) p = &def->proto;
    if (!p) continue;
    if (!isValidUnsignedUse(p->returnType)) {
      diags_.error(p->nameLoc, "invalid return type");
      return false;
    }
    if (requiresEnumDef(p->returnType)) {
      if (!enumNames.count(p->returnType.enumName)) {
        diags_.error(p->nameLoc, "unknown enum type '" + p->returnType.enumName + "'");
        return false;
      }
    }
    for (const auto& prm : p->params) {
      if (prm.type.isVoidObject()) {
        diags_.error(prm.loc, "invalid parameter type");
        return false;
      }
      if (!isValidUnsignedUse(prm.type)) {
        diags_.error(prm.loc, "invalid parameter type");
        return false;
      }
      if (isArrayElementVoid(prm.type)) {
        diags_.error(prm.loc, "invalid array element type");
        return false;
      }
      if (hasInvalidArraySize(prm.type, /*allowFirstEmpty=*/true)) {
        diags_.error(prm.loc, "invalid array size");
        return false;
      }
      if (requiresEnumDef(prm.type)) {
        if (!enumNames.count(prm.type.enumName)) {
          diags_.error(prm.loc, "unknown enum type '" + prm.type.enumName + "'");
          return false;
        }
      }
    }
  }

  Scope globalScope;

  // 2) check global variable declarations
  {
    ScopeStack scopes;
    scopes.push_back({});

    for (auto& item : tu.items) {
      auto* g = std::get_if<GlobalVarDecl>(&item);
      if (!g) continue;

      for (auto& decl : g->items) {
        if (scopes.back().count(decl.name)) {
          diags_.error(decl.nameLoc, "redefinition of '" + decl.name + "'");
          return false;
        }
        if (isArrayElementVoid(decl.type)) {
          diags_.error(decl.nameLoc, "invalid array element type");
          return false;
        }
        if (decl.type.isVoidObject()) {
          diags_.error(decl.nameLoc, "invalid use of void type");
          return false;
        }
        if (!isValidUnsignedUse(decl.type)) {
          diags_.error(decl.nameLoc, "invalid use of unsigned type");
          return false;
        }
        if (!fillArraySizeFromString(decl, diags_)) {
          return false;
        }
        if (!fillArraySizeFromInitList(decl, diags_)) {
          return false;
        }
        if (hasInvalidArraySize(decl.type, /*allowFirstEmpty=*/false)) {
          diags_.error(decl.nameLoc, "invalid array size");
          return false;
        }
        if (requiresStructDef(decl.type)) {
          if (!lookupStruct(structs, decl.type.structName)) {
            diags_.error(decl.nameLoc, "unknown struct type '" + decl.type.structName + "'");
            return false;
          }
        }
        if (requiresEnumDef(decl.type)) {
          if (!enumNames.count(decl.type.enumName)) {
            diags_.error(decl.nameLoc, "unknown enum type '" + decl.type.enumName + "'");
            return false;
          }
        }

        if (decl.initExpr) {
          bool allowArrayInit = decl.type.isArray() && !decl.type.ptrOutsideArrays;
          if (!checkInitializer(diags_, scopes, fns, structs, enumConsts,
                                decl.type, *decl.initExpr, allowArrayInit)) {
            return false;
          }
        }

        scopes.back().emplace(decl.name, decl.type);
        globalScope.emplace(decl.name, decl.type);
      }
    }
  }

  // 3) check bodies of function definitions
  for (const auto& item : tu.items) {
    auto* def = std::get_if<FunctionDef>(&item);
    if (!def) continue;

    ScopeStack scopes;
    scopes.push_back(globalScope); // globals
    scopes.push_back({});          // function scope

    // parameters as locals ONLY if they have names
    for (const auto& prm : def->proto.params) {
      if (!prm.name.has_value()) continue;
      const std::string& pname = *prm.name;
      auto& cur = scopes.back();
      if (cur.count(pname)) {
        diags_.error(prm.nameLoc, "redefinition of '" + pname + "'");
        continue;
      }
      cur.emplace(pname, adjustParamType(prm.type));
    }

    for (const auto& st : def->body) {
      checkStmtImpl(diags_, scopes, fns, structs, enumConsts, enumNames,
                    def->proto.returnType,
                    /*loopDepth=*/0, /*switchDepth=*/0, *st);
    }
  }

  return !diags_.hasError();
}

} // namespace c99cc
