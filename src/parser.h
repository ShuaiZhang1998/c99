#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "diag.h"
#include "lexer.h"

namespace c99cc {

// -------------------- AST --------------------

struct Node {
  SourceLocation loc;
  explicit Node(SourceLocation l) : loc(l) {}
  virtual ~Node() = default;
};

struct FunctionType;

enum class StorageClass {
  None,
  Static,
  Extern,
};

struct Type {
  enum class Base { Char, Short, Int, Long, LongLong, Bool, Float, Double, LongDouble, Void, Struct, Union, Enum };
  Base base = Base::Int;
  bool isUnsigned = false;
  bool isConst = false;
  std::string structName;
  std::string unionName;
  std::string enumName;
  int ptrDepth = 0; // 0 == int, 1 == int*, 2 == int**, ...
  std::vector<bool> ptrConst;
  std::vector<std::optional<size_t>> arrayDims;
  bool ptrOutsideArrays = false;
  std::shared_ptr<FunctionType> func;
  Type() = default;
  Type(Base b, int d) : base(b), ptrDepth(d) {}
  Type(Base b, int d, std::vector<std::optional<size_t>> dims)
      : base(b), ptrDepth(d), arrayDims(std::move(dims)) {}
  bool isInt() const { return base == Base::Int && ptrDepth == 0 && arrayDims.empty(); }
  bool isFloat() const { return base == Base::Float && ptrDepth == 0 && arrayDims.empty(); }
  bool isDouble() const { return base == Base::Double && ptrDepth == 0 && arrayDims.empty(); }
  bool isLongDouble() const { return base == Base::LongDouble && ptrDepth == 0 && arrayDims.empty(); }
  bool isBool() const { return base == Base::Bool && ptrDepth == 0 && arrayDims.empty(); }
  bool isFloating() const {
    return (base == Base::Float || base == Base::Double || base == Base::LongDouble) &&
           ptrDepth == 0 && arrayDims.empty();
  }
  bool isInteger() const {
    if (ptrDepth != 0 || !arrayDims.empty()) return false;
    return base == Base::Char || base == Base::Short || base == Base::Int || base == Base::Long ||
           base == Base::LongLong || base == Base::Bool || base == Base::Enum;
  }
  bool isNumeric() const { return isInteger() || isFloating(); }
  bool isVoid() const { return base == Base::Void && ptrDepth == 0 && arrayDims.empty(); }
  bool isStruct() const { return base == Base::Struct; }
  bool isUnion() const { return base == Base::Union; }
  bool isEnum() const { return base == Base::Enum; }
  bool isPointer() const { return ptrDepth > 0; }
  bool isArray() const { return !arrayDims.empty(); }
  bool isFunctionPointer() const { return func != nullptr; }
  bool isTopLevelConst() const {
    if (ptrDepth > 0) return !ptrConst.empty() && ptrConst[0];
    return isConst;
  }
  void addPointerLevel(bool isConstPtr) {
    ptrDepth++;
    ptrConst.push_back(isConstPtr);
  }
  void addPointerQuals(const std::vector<bool>& quals) {
    for (bool q : quals) addPointerLevel(q);
  }
  void clearTopLevelConst() {
    if (ptrDepth > 0) {
      if (!ptrConst.empty()) ptrConst[0] = false;
    } else {
      isConst = false;
    }
  }
  Type pointee() const {
    Type t{base, ptrDepth - 1, arrayDims};
    t.isUnsigned = isUnsigned;
    t.isConst = isConst;
    t.structName = structName;
    t.unionName = unionName;
    t.enumName = enumName;
    t.ptrOutsideArrays = false;
    t.func = func;
    if (!ptrConst.empty()) {
      t.ptrConst.assign(ptrConst.begin() + 1, ptrConst.end());
    }
    return t;
  }
  Type elementType() const {
    if (arrayDims.empty()) {
    Type t{base, ptrDepth};
    t.isUnsigned = isUnsigned;
    t.isConst = isConst;
    t.structName = structName;
    t.unionName = unionName;
    t.enumName = enumName;
    t.func = func;
    t.ptrConst = ptrConst;
    return t;
  }
    std::vector<std::optional<size_t>> rest(arrayDims.begin() + 1, arrayDims.end());
    Type t{base, ptrDepth, std::move(rest)};
    t.isUnsigned = isUnsigned;
    t.isConst = isConst;
    t.structName = structName;
    t.unionName = unionName;
    t.enumName = enumName;
    t.func = func;
    t.ptrConst = ptrConst;
    return t;
  }
  Type decayType() const {
    Type elem = elementType();
    elem.addPointerLevel(false);
    elem.ptrOutsideArrays = !elem.arrayDims.empty();
    return elem;
  }
  bool isVoidPointer() const { return base == Base::Void && ptrDepth == 1 && !func; }
  bool isVoidObject() const { return base == Base::Void && ptrDepth == 0; }
  bool operator==(const Type& other) const;
  bool operator!=(const Type& other) const { return !(*this == other); }
};

struct FunctionType {
  Type returnType;
  std::vector<Type> params;
  bool isVariadic = false;
  bool operator==(const FunctionType& other) const {
    return returnType == other.returnType && params == other.params &&
           isVariadic == other.isVariadic;
  }
  bool operator!=(const FunctionType& other) const { return !(*this == other); }
};

inline bool Type::operator==(const Type& other) const {
  if ((func && !other.func) || (!func && other.func)) return false;
  if (func && other.func) {
    if (!(*func == *other.func)) return false;
  }
  return base == other.base && isUnsigned == other.isUnsigned &&
         isConst == other.isConst && structName == other.structName &&
         unionName == other.unionName && enumName == other.enumName &&
         ptrDepth == other.ptrDepth && arrayDims == other.arrayDims &&
         ptrOutsideArrays == other.ptrOutsideArrays && ptrConst == other.ptrConst;
}

struct Declarator {
  Type type;
  std::string name;
  SourceLocation nameLoc;
};

struct Expr : Node {
  using Node::Node;
  mutable std::optional<Type> semaType;
  virtual ~Expr() = default;
};

struct Stmt : Node {
  using Node::Node;
  virtual ~Stmt() = default;
};

struct IntLiteralExpr final : Expr {
  int64_t value;
  bool isUnsigned = false;
  int longKind = 0; // 0=int, 1=long, 2=long long
  IntLiteralExpr(SourceLocation l, int64_t v, bool isU, int lk)
      : Expr(l), value(v), isUnsigned(isU), longKind(lk) {}
};

struct FloatLiteralExpr final : Expr {
  double value;
  bool isFloat = false;
  FloatLiteralExpr(SourceLocation l, double v, bool isF) : Expr(l), value(v), isFloat(isF) {}
};

struct StringLiteralExpr final : Expr {
  std::string value;
  StringLiteralExpr(SourceLocation l, std::string v) : Expr(l), value(std::move(v)) {}
};

struct Designator {
  enum class Kind { Field, Index };
  Kind kind = Kind::Index;
  SourceLocation loc;
  std::string field;
  size_t index = 0;
  static Designator fieldName(SourceLocation l, std::string name) {
    Designator d;
    d.kind = Kind::Field;
    d.loc = l;
    d.field = std::move(name);
    return d;
  }
  static Designator arrayIndex(SourceLocation l, size_t idx) {
    Designator d;
    d.kind = Kind::Index;
    d.loc = l;
    d.index = idx;
    return d;
  }
};

struct InitElem {
  SourceLocation loc;
  std::vector<Designator> designators;
  std::unique_ptr<Expr> expr;
  InitElem(SourceLocation l, std::vector<Designator> ds, std::unique_ptr<Expr> e)
      : loc(l), designators(std::move(ds)), expr(std::move(e)) {}
};

struct VarRefExpr final : Expr {
  std::string name;
  VarRefExpr(SourceLocation l, std::string n) : Expr(l), name(std::move(n)) {}
};

struct IncDecExpr final : Expr {
  bool isInc = true;
  bool isPost = false;
  std::unique_ptr<Expr> operand;
  IncDecExpr(SourceLocation l, bool inc, bool post, std::unique_ptr<Expr> e)
      : Expr(l), isInc(inc), isPost(post), operand(std::move(e)) {}
};

struct CastExpr final : Expr {
  Type targetType;
  std::unique_ptr<Expr> expr;
  CastExpr(SourceLocation l, Type t, std::unique_ptr<Expr> e)
      : Expr(l), targetType(std::move(t)), expr(std::move(e)) {}
};

struct SizeofExpr final : Expr {
  bool isType = false;
  Type type;
  std::unique_ptr<Expr> expr;
  SizeofExpr(SourceLocation l, Type t) : Expr(l), isType(true), type(std::move(t)) {}
  SizeofExpr(SourceLocation l, std::unique_ptr<Expr> e)
      : Expr(l), isType(false), expr(std::move(e)) {}
};

struct CallExpr final : Expr {
  std::string callee;
  SourceLocation calleeLoc;
  std::unique_ptr<Expr> calleeExpr;
  std::vector<std::unique_ptr<Expr>> args;
  CallExpr(SourceLocation l, std::string c, SourceLocation cLoc, std::vector<std::unique_ptr<Expr>> a)
      : Expr(l), callee(std::move(c)), calleeLoc(cLoc), args(std::move(a)) {}
  CallExpr(SourceLocation l, std::unique_ptr<Expr> cExpr, std::vector<std::unique_ptr<Expr>> a)
      : Expr(l), calleeExpr(std::move(cExpr)), args(std::move(a)) {}
};

struct UnaryExpr final : Expr {
  TokenKind op;
  std::unique_ptr<Expr> operand;
  UnaryExpr(SourceLocation l, TokenKind o, std::unique_ptr<Expr> e)
      : Expr(l), op(o), operand(std::move(e)) {}
};

struct SubscriptExpr final : Expr {
  std::unique_ptr<Expr> base;
  std::unique_ptr<Expr> index;
  SubscriptExpr(SourceLocation l, std::unique_ptr<Expr> b, std::unique_ptr<Expr> i)
      : Expr(l), base(std::move(b)), index(std::move(i)) {}
};

struct MemberExpr final : Expr {
  std::unique_ptr<Expr> base;
  std::string member;
  SourceLocation memberLoc;
  bool isArrow = false;
  MemberExpr(SourceLocation l, std::unique_ptr<Expr> b, std::string m, SourceLocation mLoc,
             bool arrow)
      : Expr(l), base(std::move(b)), member(std::move(m)), memberLoc(mLoc), isArrow(arrow) {}
};

struct InitListExpr final : Expr {
  std::vector<InitElem> elems;
  InitListExpr(SourceLocation l, std::vector<InitElem> es)
      : Expr(l), elems(std::move(es)) {}
};

struct BinaryExpr final : Expr {
  TokenKind op;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
  BinaryExpr(SourceLocation l, TokenKind o, std::unique_ptr<Expr> a, std::unique_ptr<Expr> b)
      : Expr(l), op(o), lhs(std::move(a)), rhs(std::move(b)) {}
};

struct TernaryExpr final : Expr {
  std::unique_ptr<Expr> cond;
  std::unique_ptr<Expr> thenExpr;
  std::unique_ptr<Expr> elseExpr;
  TernaryExpr(SourceLocation l, std::unique_ptr<Expr> c, std::unique_ptr<Expr> t,
              std::unique_ptr<Expr> e)
      : Expr(l), cond(std::move(c)), thenExpr(std::move(t)), elseExpr(std::move(e)) {}
};

struct AssignExpr final : Expr {
  TokenKind op;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
  AssignExpr(SourceLocation l, TokenKind o, std::unique_ptr<Expr> left, std::unique_ptr<Expr> r)
      : Expr(l), op(o), lhs(std::move(left)), rhs(std::move(r)) {}
};

// statements
struct DeclItem {
  Type type;
  std::string name;
  SourceLocation nameLoc;
  std::unique_ptr<Expr> initExpr; // nullable
  StorageClass storage = StorageClass::None;
};

struct StructField {
  Type type;
  std::string name;
  SourceLocation nameLoc;
  std::optional<int64_t> bitWidth;
};

struct DeclStmt final : Stmt {
  std::vector<DeclItem> items;
  DeclStmt(SourceLocation l, std::vector<DeclItem> d)
      : Stmt(l), items(std::move(d)) {}
};

struct TypedefStmt final : Stmt {
  std::vector<DeclItem> items;
  TypedefStmt(SourceLocation l, std::vector<DeclItem> d)
      : Stmt(l), items(std::move(d)) {}
};

struct AssignStmt final : Stmt {
  std::string name;
  SourceLocation nameLoc;
  std::unique_ptr<Expr> valueExpr;
  AssignStmt(SourceLocation l, std::string n, SourceLocation nLoc, std::unique_ptr<Expr> v)
      : Stmt(l), name(std::move(n)), nameLoc(nLoc), valueExpr(std::move(v)) {}
};

struct ReturnStmt final : Stmt {
  std::unique_ptr<Expr> valueExpr; // nullable for 'return;'
  ReturnStmt(SourceLocation l, std::unique_ptr<Expr> v) : Stmt(l), valueExpr(std::move(v)) {}
  explicit ReturnStmt(SourceLocation l) : Stmt(l) {}
};

struct ExprStmt final : Stmt {
  std::unique_ptr<Expr> expr;
  ExprStmt(SourceLocation l, std::unique_ptr<Expr> e) : Stmt(l), expr(std::move(e)) {}
};

struct EmptyStmt final : Stmt {
  explicit EmptyStmt(SourceLocation l) : Stmt(l) {}
};

struct BreakStmt final : Stmt {
  explicit BreakStmt(SourceLocation l) : Stmt(l) {}
};

struct ContinueStmt final : Stmt {
  explicit ContinueStmt(SourceLocation l) : Stmt(l) {}
};

struct BlockStmt final : Stmt {
  std::vector<std::unique_ptr<Stmt>> stmts;
  BlockStmt(SourceLocation l, std::vector<std::unique_ptr<Stmt>> s)
      : Stmt(l), stmts(std::move(s)) {}
};

struct IfStmt final : Stmt {
  std::unique_ptr<Expr> cond;
  std::unique_ptr<Stmt> thenBranch;
  std::unique_ptr<Stmt> elseBranch; // nullable
  IfStmt(SourceLocation l, std::unique_ptr<Expr> c, std::unique_ptr<Stmt> t, std::unique_ptr<Stmt> e)
      : Stmt(l), cond(std::move(c)), thenBranch(std::move(t)), elseBranch(std::move(e)) {}
};

struct WhileStmt final : Stmt {
  std::unique_ptr<Expr> cond;
  std::unique_ptr<Stmt> body;
  WhileStmt(SourceLocation l, std::unique_ptr<Expr> c, std::unique_ptr<Stmt> b)
      : Stmt(l), cond(std::move(c)), body(std::move(b)) {}
};

struct DoWhileStmt final : Stmt {
  std::unique_ptr<Stmt> body;
  std::unique_ptr<Expr> cond;
  DoWhileStmt(SourceLocation l, std::unique_ptr<Stmt> b, std::unique_ptr<Expr> c)
      : Stmt(l), body(std::move(b)), cond(std::move(c)) {}
};

struct ForStmt final : Stmt {
  std::unique_ptr<Stmt> init;   // nullable
  std::unique_ptr<Expr> cond;   // nullable
  std::unique_ptr<Expr> inc;    // nullable
  std::unique_ptr<Stmt> body;
  ForStmt(SourceLocation l, std::unique_ptr<Stmt> i, std::unique_ptr<Expr> c,
          std::unique_ptr<Expr> in, std::unique_ptr<Stmt> b)
      : Stmt(l), init(std::move(i)), cond(std::move(c)), inc(std::move(in)), body(std::move(b)) {}
};

struct SwitchCase {
  std::optional<int64_t> value; // nullopt for default
  SourceLocation loc;
  std::vector<std::unique_ptr<Stmt>> stmts;
};

struct SwitchStmt final : Stmt {
  std::unique_ptr<Expr> cond;
  std::vector<SwitchCase> cases;
  SwitchStmt(SourceLocation l, std::unique_ptr<Expr> c, std::vector<SwitchCase> cs)
      : Stmt(l), cond(std::move(c)), cases(std::move(cs)) {}
};

// ---- functions / TU ----

struct Param {
  Type type;
  // param name is optional (prototype can omit names)
  std::optional<std::string> name;
  SourceLocation nameLoc; // valid iff name.has_value()
  SourceLocation loc;     // location of 'int' keyword for this param
};

struct FunctionProto {
  Type returnType;
  std::string name;
  SourceLocation nameLoc;
  std::vector<Param> params;
  bool isVariadic = false;
  StorageClass storage = StorageClass::None;
};

struct FunctionDecl {
  FunctionProto proto;
  SourceLocation semiLoc; // location of ';'
};

struct FunctionDef {
  FunctionProto proto;
  std::vector<std::unique_ptr<Stmt>> body;
};

struct GlobalVarDecl {
  std::vector<DeclItem> items;
};

struct StructDef {
  std::string name;
  SourceLocation nameLoc;
  std::vector<StructField> fields;
};

struct UnionDef {
  std::string name;
  SourceLocation nameLoc;
  std::vector<StructField> fields;
};

struct EnumItem {
  std::string name;
  SourceLocation nameLoc;
  int64_t value = 0;
};

struct EnumDef {
  std::optional<std::string> name;
  SourceLocation nameLoc;
  std::vector<EnumItem> items;
};

struct TypedefDecl {
  std::vector<DeclItem> items;
};

using TopLevelItem = std::variant<StructDef, UnionDef, EnumDef, TypedefDecl,
                                  FunctionDecl, FunctionDef, GlobalVarDecl>;

struct AstTranslationUnit {
  std::vector<TopLevelItem> items;
};

// -------------------- Parser --------------------

class Parser {
public:
  Parser(Lexer& lex, Diagnostics& diags) : lex_(lex), diags_(diags) { cur_ = lex_.next(); }
  std::optional<AstTranslationUnit> parse() { return parseTranslationUnit(); }

private:
  Lexer& lex_;
  Diagnostics& diags_;
  Token cur_;
  Token peek_{};
  bool hasPeek_ = false;
  std::vector<TopLevelItem> pending_;
  std::unordered_map<std::string, Type> typedefs_;
  std::unordered_map<std::string, int64_t> enumConstants_;

  void advance();
  bool expect(TokenKind k, const char* what);
  const Token& peekToken();
  struct PtrQuals {
    int depth = 0;
    std::vector<bool> consts;
  };
  PtrQuals parsePointerQuals();
  struct ParsedTypeSpec {
    Type type;
    std::optional<StructDef> structDef;
    std::optional<UnionDef> unionDef;
    std::optional<EnumDef> enumDef;
    StorageClass storage = StorageClass::None;
  };
  std::optional<ParsedTypeSpec> parseTypeSpec(bool allowStructDef, bool allowStorage);
  std::optional<Type> parseTypeName(bool allowStructDef);
  std::optional<std::vector<StructField>> parseStructFields();
  std::optional<std::vector<StructField>> parseUnionFields();
  std::optional<std::vector<EnumItem>> parseEnumItems();
  std::optional<std::vector<std::optional<size_t>>> parseArrayDims(bool allowFirstEmpty);
  std::optional<std::unique_ptr<Expr>> parseInitializer();
  bool evalConstExpr(const Expr& e, int64_t& out);

  std::optional<AstTranslationUnit> parseTranslationUnit();
  std::optional<TopLevelItem> parseTopLevelItem();
  std::optional<FunctionProto> parseFunctionProto();         // parses: int name '(' params ')'
  struct ParamList {
    std::vector<Param> params;
    bool isVariadic = false;
  };
  std::optional<ParamList> parseParamList();        // parses inside (...) ; allows nameless params
  std::optional<FunctionDef> parseFunctionDefAfterProto(FunctionProto proto);

  std::optional<std::unique_ptr<Stmt>> parseStmt();
  std::optional<std::unique_ptr<Stmt>> parseDeclStmt();
  std::optional<std::unique_ptr<Stmt>> parseTypedefStmt();
  std::optional<std::unique_ptr<Stmt>> parseAssignStmt(); // legacy, may be unused
  std::optional<std::unique_ptr<Stmt>> parseReturnStmt();
  std::optional<std::unique_ptr<Stmt>> parseBreakStmt();
  std::optional<std::unique_ptr<Stmt>> parseContinueStmt();
  std::optional<std::unique_ptr<Stmt>> parseBlockStmt();
  std::optional<std::unique_ptr<Stmt>> parseIfStmt();
  std::optional<std::unique_ptr<Stmt>> parseWhileStmt();
  std::optional<std::unique_ptr<Stmt>> parseDoWhileStmt();
  std::optional<std::unique_ptr<Stmt>> parseForStmt();
  std::optional<std::unique_ptr<Stmt>> parseSwitchStmt();

  std::optional<std::unique_ptr<Expr>> parseExpr();           // comma-expression
  std::optional<std::unique_ptr<Expr>> parseAssignmentExpr(); // assignment (NO comma)
  std::optional<std::unique_ptr<Expr>> parseConditionalExpr();
  std::optional<std::unique_ptr<Expr>> parseUnary();
  std::optional<std::unique_ptr<Expr>> parsePostfix();
  std::optional<std::unique_ptr<Expr>> parsePrimary();
  std::optional<Declarator> parseDeclarator(const Type& baseType, bool allowArray,
                                            bool allowFirstEmpty, bool allowFunctionPointer);

  // Compatibility (not used)
  std::optional<std::unique_ptr<Expr>> parseBinary(int /*minPrec*/);

  int precedence(TokenKind k) const;
  bool isBinaryOp(TokenKind k) const;

  std::optional<std::vector<DeclItem>> parseTypedefItems(bool allowStructDef);
  std::optional<TopLevelItem> parseTypedefTopLevel();
};

} // namespace c99cc
