#pragma once
#include <string>
#include <optional>
#include <cstdint>
#include <memory>
#include <vector>

#include "lexer.h"
#include "diag.h"

namespace c99cc {

//====================
// AST: Expressions
//====================
struct Expr {
  SourceLocation loc{};
  explicit Expr(SourceLocation l) : loc(l) {}
  virtual ~Expr() = default;
};

struct IntLiteralExpr final : Expr {
  int64_t value;
  IntLiteralExpr(SourceLocation l, int64_t v) : Expr(l), value(v) {}
};

struct VarRefExpr final : Expr {
  std::string name;
  VarRefExpr(SourceLocation l, std::string n) : Expr(l), name(std::move(n)) {}
};

struct UnaryExpr final : Expr {
  TokenKind op;                // Plus / Minus / Bang / Tilde
  std::unique_ptr<Expr> operand;

  UnaryExpr(SourceLocation l, TokenKind op, std::unique_ptr<Expr> e)
      : Expr(l), op(op), operand(std::move(e)) {}
};

struct BinaryExpr final : Expr {
  TokenKind op;
  std::unique_ptr<Expr> lhs;
  std::unique_ptr<Expr> rhs;

  BinaryExpr(SourceLocation l, TokenKind op, std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs)
      : Expr(l), op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
};

//====================
// AST: Statements
//====================
struct Stmt {
  SourceLocation loc{};
  explicit Stmt(SourceLocation l) : loc(l) {}
  virtual ~Stmt() = default;
};

// Decl: int x;  or  int x = <expr>;
struct DeclStmt final : Stmt {
  std::string name;
  SourceLocation nameLoc{};
  std::unique_ptr<Expr> initExpr; // nullable

  DeclStmt(SourceLocation l, std::string n, SourceLocation nl, std::unique_ptr<Expr> init)
      : Stmt(l), name(std::move(n)), nameLoc(nl), initExpr(std::move(init)) {}
};

// Assign: x = <expr>;
struct AssignStmt final : Stmt {
  std::string name;
  SourceLocation nameLoc{};
  std::unique_ptr<Expr> valueExpr;

  AssignStmt(SourceLocation l, std::string n, SourceLocation nl, std::unique_ptr<Expr> e)
      : Stmt(l), name(std::move(n)), nameLoc(nl), valueExpr(std::move(e)) {}
};

struct ReturnStmt final : Stmt {
  std::unique_ptr<Expr> valueExpr;
  ReturnStmt(SourceLocation l, std::unique_ptr<Expr> e) : Stmt(l), valueExpr(std::move(e)) {}
};

//====================
// TU
//====================
struct AstTranslationUnit {
  std::string funcName;
  std::vector<std::unique_ptr<Stmt>> body;
};

class Parser {
public:
  Parser(Lexer& lex, Diagnostics& diags);
  std::optional<AstTranslationUnit> parse();

private:
  std::optional<std::unique_ptr<Stmt>> parseStmt();
  std::optional<std::unique_ptr<Stmt>> parseDeclStmt();   // "int" ident ["=" expr] ";"
  std::optional<std::unique_ptr<Stmt>> parseReturnStmt(); // "return" expr ";"
  std::optional<std::unique_ptr<Stmt>> parseAssignStmt(); // ident "=" expr ";"

  // expressions
  std::unique_ptr<Expr> parseExpr(int minPrec = 0);
  std::unique_ptr<Expr> parseUnary();
  std::unique_ptr<Expr> parsePrimary();
  int precedence(TokenKind k) const;

  bool expect(TokenKind k, const char* what);
  void advance();

  Lexer& lex_;
  Diagnostics& diags_;
  Token cur_;
};

} // namespace c99cc
