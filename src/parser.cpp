#include "parser.h"

namespace c99cc {

bool Parser::expect(TokenKind k, const char* what) {
  if (cur_.kind == k) return true;
  diags_.error(cur_.loc, std::string("expected ") + what);
  return false;
}

std::optional<AstTranslationUnit> Parser::parseTranslationUnit() {
  // int <name>() { <stmts> }
  if (!expect(TokenKind::KwInt, "'int'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::Identifier, "identifier")) return std::nullopt;
  std::string fnName = cur_.text;
  advance();

  if (!expect(TokenKind::LParen, "'('")) return std::nullopt;
  advance();
  if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::LBrace, "'{'")) return std::nullopt;
  advance();

  std::vector<std::unique_ptr<Stmt>> body;
  while (cur_.kind != TokenKind::RBrace && cur_.kind != TokenKind::Eof) {
    auto s = parseStmt();
    if (!s) return std::nullopt;
    body.push_back(std::move(*s));
  }

  if (!expect(TokenKind::RBrace, "'}'")) return std::nullopt;
  advance();

  AstTranslationUnit tu;
  tu.funcName = std::move(fnName);
  tu.body = std::move(body);
  return tu;
}

std::optional<std::unique_ptr<Stmt>> Parser::parseStmt() {
  if (cur_.kind == TokenKind::KwInt) return parseDeclStmt();
  if (cur_.kind == TokenKind::KwReturn) return parseReturnStmt();
  if (cur_.kind == TokenKind::KwIf) return parseIfStmt();
  if (cur_.kind == TokenKind::KwWhile) return parseWhileStmt();
  if (cur_.kind == TokenKind::LBrace) return parseBlockStmt();

  if (cur_.kind == TokenKind::Identifier) return parseAssignStmt();

  diags_.error(cur_.loc, "expected statement");
  return std::nullopt;
}

std::optional<std::unique_ptr<Stmt>> Parser::parseBlockStmt() {
  SourceLocation l = cur_.loc;
  if (!expect(TokenKind::LBrace, "'{'")) return std::nullopt;
  advance();

  std::vector<std::unique_ptr<Stmt>> stmts;
  while (cur_.kind != TokenKind::RBrace && cur_.kind != TokenKind::Eof) {
    auto s = parseStmt();
    if (!s) return std::nullopt;
    stmts.push_back(std::move(*s));
  }

  if (!expect(TokenKind::RBrace, "'}'")) return std::nullopt;
  advance();

  return std::make_unique<BlockStmt>(l, std::move(stmts));
}

std::optional<std::unique_ptr<Stmt>> Parser::parseIfStmt() {
  SourceLocation ifLoc = cur_.loc;
  if (!expect(TokenKind::KwIf, "'if'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::LParen, "'('")) return std::nullopt;
  advance();

  auto cond = parseExpr();
  if (!cond) return std::nullopt;

  if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
  advance();

  auto thenS = parseStmt();
  if (!thenS) return std::nullopt;

  std::unique_ptr<Stmt> elseS = nullptr;
  if (cur_.kind == TokenKind::KwElse) {
    advance();
    auto e = parseStmt();
    if (!e) return std::nullopt;
    elseS = std::move(*e);
  }

  return std::make_unique<IfStmt>(ifLoc, std::move(*cond), std::move(*thenS), std::move(elseS));
}

std::optional<std::unique_ptr<Stmt>> Parser::parseWhileStmt() {
  SourceLocation wLoc = cur_.loc;
  if (!expect(TokenKind::KwWhile, "'while'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::LParen, "'('")) return std::nullopt;
  advance();

  auto cond = parseExpr();
  if (!cond) return std::nullopt;

  if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
  advance();

  auto body = parseStmt();
  if (!body) return std::nullopt;

  return std::make_unique<WhileStmt>(wLoc, std::move(*cond), std::move(*body));
}

std::optional<std::unique_ptr<Stmt>> Parser::parseDeclStmt() {
  // int <name> ["=" expr] ";"
  SourceLocation l = cur_.loc;
  if (!expect(TokenKind::KwInt, "'int'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::Identifier, "identifier")) return std::nullopt;
  std::string name = cur_.text;
  SourceLocation nameLoc = cur_.loc;
  advance();

  std::unique_ptr<Expr> init = nullptr;
  if (cur_.kind == TokenKind::Assign) {
    advance();
    auto e = parseExpr();
    if (!e) return std::nullopt;
    init = std::move(*e);
  }

  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();

  return std::make_unique<DeclStmt>(l, std::move(name), nameLoc, std::move(init));
}

std::optional<std::unique_ptr<Stmt>> Parser::parseAssignStmt() {
  // <name> "=" expr ";"
  SourceLocation l = cur_.loc;

  if (!expect(TokenKind::Identifier, "identifier")) return std::nullopt;
  std::string name = cur_.text;
  SourceLocation nameLoc = cur_.loc;
  advance();

  if (!expect(TokenKind::Assign, "'='")) return std::nullopt;
  advance();

  auto rhs = parseExpr();
  if (!rhs) return std::nullopt;

  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();

  return std::make_unique<AssignStmt>(l, std::move(name), nameLoc, std::move(*rhs));
}

std::optional<std::unique_ptr<Stmt>> Parser::parseReturnStmt() {
  SourceLocation l = cur_.loc;
  if (!expect(TokenKind::KwReturn, "'return'")) return std::nullopt;
  advance();

  auto e = parseExpr();
  if (!e) return std::nullopt;

  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();

  return std::make_unique<ReturnStmt>(l, std::move(*e));
}

int Parser::precedence(TokenKind k) const {
  switch (k) {
    case TokenKind::Star:
    case TokenKind::Slash:
      return 20;
    case TokenKind::Plus:
    case TokenKind::Minus:
      return 10;
    case TokenKind::Less:
    case TokenKind::Greater:
    case TokenKind::LessEqual:
    case TokenKind::GreaterEqual:
    case TokenKind::EqualEqual:
    case TokenKind::BangEqual:
      return 5;
    case TokenKind::AmpAmp:
      return 3;
    case TokenKind::PipePipe:
      return 2;
    default:
      return -1;
  }
}

bool Parser::isBinaryOp(TokenKind k) const {
  return precedence(k) >= 0;
}

std::optional<std::unique_ptr<Expr>> Parser::parsePrimary() {
  if (cur_.kind == TokenKind::IntegerLiteral) {
    SourceLocation l = cur_.loc;
    int64_t v = std::stoll(cur_.text);
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
    auto e = parseExpr();
    if (!e) return std::nullopt;
    if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
    advance();
    return e;
  }

  diags_.error(cur_.loc, "expected primary expression");
  return std::nullopt;
}

std::optional<std::unique_ptr<Expr>> Parser::parseUnary() {
  if (cur_.kind == TokenKind::Plus || cur_.kind == TokenKind::Minus ||
      cur_.kind == TokenKind::Bang || cur_.kind == TokenKind::Tilde) {
    SourceLocation l = cur_.loc;
    TokenKind op = cur_.kind;
    advance();
    auto rhs = parseUnary();
    if (!rhs) return std::nullopt;
    return std::make_unique<UnaryExpr>(l, op, std::move(*rhs));
  }
  return parsePrimary();
}

std::optional<std::unique_ptr<Expr>> Parser::parseBinary(int minPrec) {
  auto lhs = parseUnary();
  if (!lhs) return std::nullopt;

  while (isBinaryOp(cur_.kind) && precedence(cur_.kind) >= minPrec) {
    TokenKind op = cur_.kind;
    int prec = precedence(op);
    advance();

    auto rhs = parseBinary(prec + 1); // left-assoc
    if (!rhs) return std::nullopt;

    lhs = std::make_unique<BinaryExpr>((*lhs)->loc, op, std::move(*lhs), std::move(*rhs));
  }

  return lhs;
}

std::optional<std::unique_ptr<Expr>> Parser::parseExpr() {
  // Parse non-assignment first (||, &&, comparisons, +-, */ ...)
  auto lhs = parseBinary(0);
  if (!lhs) return std::nullopt;

  // assignment: only allow "VarRef = expr" and it is right-assoc
  if (cur_.kind == TokenKind::Assign) {
    auto* vr = dynamic_cast<VarRefExpr*>(lhs->get());
    if (!vr) {
      diags_.error(cur_.loc, "expected identifier on left-hand side of assignment");
      return std::nullopt;
    }

    SourceLocation nameLoc = vr->loc;
    std::string name = vr->name;
    SourceLocation assignLoc = cur_.loc;

    advance();
    auto rhs = parseExpr(); // right-assoc
    if (!rhs) return std::nullopt;

    return std::make_unique<AssignExpr>(assignLoc, std::move(name), nameLoc, std::move(*rhs));
  }

  return lhs;
}

} // namespace c99cc
