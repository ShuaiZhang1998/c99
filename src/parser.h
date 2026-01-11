#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
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

struct Expr : Node {
  using Node::Node;
  virtual ~Expr() = default;
};

struct Stmt : Node {
  using Node::Node;
  virtual ~Stmt() = default;
};

struct IntLiteralExpr final : Expr {
  int64_t value;
  IntLiteralExpr(SourceLocation l, int64_t v) : Expr(l), value(v) {}
};

struct VarRefExpr final : Expr {
  std::string name;
  VarRefExpr(SourceLocation l, std::string n) : Expr(l), name(std::move(n)) {}
};

struct CallExpr final : Expr {
  std::string callee;
  SourceLocation calleeLoc;
  std::vector<std::unique_ptr<Expr>> args;
  CallExpr(SourceLocation l, std::string c, SourceLocation cLoc, std::vector<std::unique_ptr<Expr>> a)
      : Expr(l), callee(std::move(c)), calleeLoc(cLoc), args(std::move(a)) {}
};

struct UnaryExpr final : Expr {
  TokenKind op;
  std::unique_ptr<Expr> operand;
  UnaryExpr(SourceLocation l, TokenKind o, std::unique_ptr<Expr> e)
      : Expr(l), op(o), operand(std::move(e)) {}
};

struct BinaryExpr final : Expr {
  TokenKind op;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;
  BinaryExpr(SourceLocation l, TokenKind o, std::unique_ptr<Expr> a, std::unique_ptr<Expr> b)
      : Expr(l), op(o), lhs(std::move(a)), rhs(std::move(b)) {}
};

struct AssignExpr final : Expr {
  std::string name;
  SourceLocation nameLoc;
  std::unique_ptr<Expr> rhs;
  AssignExpr(SourceLocation l, std::string n, SourceLocation nLoc, std::unique_ptr<Expr> r)
      : Expr(l), name(std::move(n)), nameLoc(nLoc), rhs(std::move(r)) {}
};

// statements
struct DeclStmt final : Stmt {
  std::string name;
  SourceLocation nameLoc;
  std::unique_ptr<Expr> initExpr; // nullable
  DeclStmt(SourceLocation l, std::string n, SourceLocation nLoc, std::unique_ptr<Expr> init)
      : Stmt(l), name(std::move(n)), nameLoc(nLoc), initExpr(std::move(init)) {}
};

struct AssignStmt final : Stmt {
  std::string name;
  SourceLocation nameLoc;
  std::unique_ptr<Expr> valueExpr;
  AssignStmt(SourceLocation l, std::string n, SourceLocation nLoc, std::unique_ptr<Expr> v)
      : Stmt(l), name(std::move(n)), nameLoc(nLoc), valueExpr(std::move(v)) {}
};

struct ReturnStmt final : Stmt {
  std::unique_ptr<Expr> valueExpr;
  ReturnStmt(SourceLocation l, std::unique_ptr<Expr> v) : Stmt(l), valueExpr(std::move(v)) {}
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

// ---- functions / TU ----

struct Param {
  // param name is optional (prototype can omit names)
  std::optional<std::string> name;
  SourceLocation nameLoc; // valid iff name.has_value()
  SourceLocation loc;     // location of 'int' keyword for this param
};

struct FunctionProto {
  std::string name;
  SourceLocation nameLoc;
  std::vector<Param> params;
};

struct FunctionDecl {
  FunctionProto proto;
  SourceLocation semiLoc; // location of ';'
};

struct FunctionDef {
  FunctionProto proto;
  std::vector<std::unique_ptr<Stmt>> body;
};

using TopLevelItem = std::variant<FunctionDecl, FunctionDef>;

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

  void advance() { cur_ = lex_.next(); }
  bool expect(TokenKind k, const char* what);

  std::optional<AstTranslationUnit> parseTranslationUnit();
  std::optional<TopLevelItem> parseTopLevelItem();
  std::optional<FunctionProto> parseFunctionProto();         // parses: int name '(' params ')'
  std::optional<std::vector<Param>> parseParamList();        // parses inside (...) ; allows nameless params
  std::optional<FunctionDef> parseFunctionDefAfterProto(FunctionProto proto);

  std::optional<std::unique_ptr<Stmt>> parseStmt();
  std::optional<std::unique_ptr<Stmt>> parseDeclStmt();
  std::optional<std::unique_ptr<Stmt>> parseAssignStmt(); // legacy, may be unused
  std::optional<std::unique_ptr<Stmt>> parseReturnStmt();
  std::optional<std::unique_ptr<Stmt>> parseBreakStmt();
  std::optional<std::unique_ptr<Stmt>> parseContinueStmt();
  std::optional<std::unique_ptr<Stmt>> parseBlockStmt();
  std::optional<std::unique_ptr<Stmt>> parseIfStmt();
  std::optional<std::unique_ptr<Stmt>> parseWhileStmt();
  std::optional<std::unique_ptr<Stmt>> parseDoWhileStmt();
  std::optional<std::unique_ptr<Stmt>> parseForStmt();

  std::optional<std::unique_ptr<Expr>> parseExpr();           // comma-expression
  std::optional<std::unique_ptr<Expr>> parseAssignmentExpr(); // assignment (NO comma)
  std::optional<std::unique_ptr<Expr>> parseUnary();
  std::optional<std::unique_ptr<Expr>> parsePrimary();

  // Compatibility (not used)
  std::optional<std::unique_ptr<Expr>> parseBinary(int /*minPrec*/);

  int precedence(TokenKind k) const;
  bool isBinaryOp(TokenKind k) const;
};

} // namespace c99cc
