#include "parser.h"
#include <cstdlib> // strtoll

namespace c99cc {

Parser::Parser(Lexer& lex, Diagnostics& diags)
  : lex_(lex), diags_(diags), cur_(lex_.next()) {}

void Parser::advance() { cur_ = lex_.next(); }

bool Parser::expect(TokenKind k, const char* what) {
  if (cur_.kind == k) return true;
  diags_.error(cur_.loc, std::string("expected ") + what);
  return false;
}

int Parser::precedence(TokenKind k) const {
  switch (k) {
    case TokenKind::Star:
    case TokenKind::Slash:
      return 20;
    case TokenKind::Plus:
    case TokenKind::Minus:
      return 10;
    default:
      return -1;
  }
}

std::unique_ptr<Expr> Parser::parsePrimary() {
  if (cur_.kind == TokenKind::IntegerLiteral) {
    SourceLocation l = cur_.loc;
    int64_t v = std::strtoll(cur_.text.c_str(), nullptr, 10);
    advance();
    return std::make_unique<IntLiteralExpr>(l, v);
  }

  if (cur_.kind == TokenKind::Identifier) {
    SourceLocation l = cur_.loc;
    std::string name = cur_.text;
    advance();
    return std::make_unique<VarRefExpr>(l, std::move(name));
  }

  if (cur_.kind == TokenKind::LParen) {
    advance();
    auto e = parseExpr(/*minPrec=*/0);
    if (!e) return nullptr;
    if (!expect(TokenKind::RParen, "')'")) return nullptr;
    advance();
    return e;
  }

  diags_.error(cur_.loc, "expected primary expression");
  return nullptr;
}

std::unique_ptr<Expr> Parser::parseExpr(int minPrec) {
  auto lhs = parsePrimary();
  if (!lhs) return nullptr;

  while (true) {
    int prec = precedence(cur_.kind);
    if (prec < minPrec) break;

    SourceLocation opLoc = cur_.loc;
    TokenKind op = cur_.kind;
    advance();

    auto rhs = parseExpr(prec + 1);
    if (!rhs) return nullptr;

    lhs = std::make_unique<BinaryExpr>(opLoc, op, std::move(lhs), std::move(rhs));
  }

  return lhs;
}

std::optional<std::unique_ptr<Stmt>> Parser::parseDeclStmt() {
  // "int" ident ["=" expr] ";"
  SourceLocation kwLoc = cur_.loc;
  if (!expect(TokenKind::KwInt, "'int'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::Identifier, "identifier")) return std::nullopt;
  SourceLocation nameLoc = cur_.loc;
  std::string name = cur_.text;
  advance();

  std::unique_ptr<Expr> init = nullptr;
  if (cur_.kind == TokenKind::Assign) {
    advance();
    init = parseExpr(/*minPrec=*/0);
    if (!init) return std::nullopt;
  }

  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();

  return std::make_unique<DeclStmt>(kwLoc, std::move(name), nameLoc, std::move(init));
}

std::optional<std::unique_ptr<Stmt>> Parser::parseReturnStmt() {
  // "return" expr ";"
  SourceLocation retLoc = cur_.loc;
  if (!expect(TokenKind::KwReturn, "'return'")) return std::nullopt;
  advance();

  auto e = parseExpr(/*minPrec=*/0);
  if (!e) return std::nullopt;

  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();

  return std::make_unique<ReturnStmt>(retLoc, std::move(e));
}

std::optional<std::unique_ptr<Stmt>> Parser::parseAssignStmt() {
  // ident "=" expr ";"
  SourceLocation stmtLoc = cur_.loc;

  if (!expect(TokenKind::Identifier, "identifier")) return std::nullopt;
  SourceLocation nameLoc = cur_.loc;
  std::string name = cur_.text;
  advance();

  if (!expect(TokenKind::Assign, "'='")) return std::nullopt;
  advance();

  auto e = parseExpr(/*minPrec=*/0);
  if (!e) return std::nullopt;

  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();

  return std::make_unique<AssignStmt>(stmtLoc, std::move(name), nameLoc, std::move(e));
}

std::optional<std::unique_ptr<Stmt>> Parser::parseStmt() {
  if (cur_.kind == TokenKind::KwInt) return parseDeclStmt();
  if (cur_.kind == TokenKind::KwReturn) return parseReturnStmt();
  if (cur_.kind == TokenKind::Identifier) return parseAssignStmt();

  diags_.error(cur_.loc, "expected statement");
  return std::nullopt;
}

std::optional<AstTranslationUnit> Parser::parse() {
  // TU := "int" IDENT "(" ")" "{" { stmt } "}"
  AstTranslationUnit tu;

  if (!expect(TokenKind::KwInt, "'int'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::Identifier, "function name")) return std::nullopt;
  tu.funcName = cur_.text;
  advance();

  if (!expect(TokenKind::LParen, "'('")) return std::nullopt;
  advance();
  if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::LBrace, "'{'")) return std::nullopt;
  advance();

  while (cur_.kind != TokenKind::RBrace && cur_.kind != TokenKind::Eof) {
    auto s = parseStmt();
    if (!s.has_value() || diags_.hasError()) return std::nullopt;
    tu.body.push_back(std::move(*s));
  }

  if (!expect(TokenKind::RBrace, "'}'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::Eof, "end of file")) return std::nullopt;
  return tu;
}

} // namespace c99cc
