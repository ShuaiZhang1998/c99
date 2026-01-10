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
  if (cur_.kind == TokenKind::KwBreak) return parseBreakStmt();
  if (cur_.kind == TokenKind::KwContinue) return parseContinueStmt();
  if (cur_.kind == TokenKind::LBrace) return parseBlockStmt();

  // assignment statement: <ident> "=" <expr> ";"
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

std::optional<std::unique_ptr<Stmt>> Parser::parseBreakStmt() {
  SourceLocation l = cur_.loc;
  if (!expect(TokenKind::KwBreak, "'break'")) return std::nullopt;
  advance();
  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();
  return std::make_unique<BreakStmt>(l);
}

std::optional<std::unique_ptr<Stmt>> Parser::parseContinueStmt() {
  SourceLocation l = cur_.loc;
  if (!expect(TokenKind::KwContinue, "'continue'")) return std::nullopt;
  advance();
  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();
  return std::make_unique<ContinueStmt>(l);
}

// -------------------- Expression parsing (stable layered) --------------------
//
// Grammar (subset):
//   expr            := assignment
//   assignment      := logical_or ( '=' assignment )?
//   logical_or      := logical_and ( '||' logical_and )*
//   logical_and     := equality ( '&&' equality )*
//   equality        := relational ( ( '==' | '!=' ) relational )*
//   relational      := additive ( ( '<' | '<=' | '>' | '>=' ) additive )*
//   additive        := multiplicative ( ( '+' | '-' ) multiplicative )*
//   multiplicative  := unary ( ( '*' | '/' ) unary )*
//   unary           := ( '+' | '-' | '!' | '~' ) unary | primary
//   primary         := integer | identifier | '(' expr ')'
//
// Notes:
// - All binary ops here are LEFT associative via ( ... )*
// - Assignment is RIGHT associative via recursion on RHS.

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

static bool isMulOp(TokenKind k) { return k == TokenKind::Star || k == TokenKind::Slash; }
static bool isAddOp(TokenKind k) { return k == TokenKind::Plus || k == TokenKind::Minus; }
static bool isRelOp(TokenKind k) {
  return k == TokenKind::Less || k == TokenKind::LessEqual ||
         k == TokenKind::Greater || k == TokenKind::GreaterEqual;
}
static bool isEqOp(TokenKind k) { return k == TokenKind::EqualEqual || k == TokenKind::BangEqual; }

static bool isAndOp(TokenKind k) { return k == TokenKind::AmpAmp; }
static bool isOrOp(TokenKind k) { return k == TokenKind::PipePipe; }

std::optional<std::unique_ptr<Expr>> Parser::parseBinary(int /*minPrec*/) {
  // Not used in layered version. Keep for header compatibility.
  return parseUnary();
}

std::optional<std::unique_ptr<Expr>> Parser::parseExpr() {
  // expr := assignment
  auto parseLogicalAnd = [&]() -> std::optional<std::unique_ptr<Expr>> {
    auto parseRelational = [&]() -> std::optional<std::unique_ptr<Expr>> {
      auto parseAdditive = [&]() -> std::optional<std::unique_ptr<Expr>> {
        auto parseMultiplicative = [&]() -> std::optional<std::unique_ptr<Expr>> {
          auto lhs = parseUnary();
          if (!lhs) return std::nullopt;
          while (isMulOp(cur_.kind)) {
            TokenKind op = cur_.kind;
            SourceLocation l = (*lhs)->loc;
            advance();
            auto rhs = parseUnary();
            if (!rhs) return std::nullopt;
            lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
          }
          return lhs;
        };

        auto lhs = parseMultiplicative();
        if (!lhs) return std::nullopt;
        while (isAddOp(cur_.kind)) {
          TokenKind op = cur_.kind;
          SourceLocation l = (*lhs)->loc;
          advance();
          auto rhs = parseMultiplicative();
          if (!rhs) return std::nullopt;
          lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
        }
        return lhs;
      };

      auto lhs = parseAdditive();
      if (!lhs) return std::nullopt;
      while (isRelOp(cur_.kind)) {
        TokenKind op = cur_.kind;
        SourceLocation l = (*lhs)->loc;
        advance();
        auto rhs = parseAdditive();
        if (!rhs) return std::nullopt;
        lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
      }
      return lhs;
    };

    auto parseEquality = [&]() -> std::optional<std::unique_ptr<Expr>> {
      auto lhs = parseRelational();
      if (!lhs) return std::nullopt;
      while (isEqOp(cur_.kind)) {
        TokenKind op = cur_.kind;
        SourceLocation l = (*lhs)->loc;
        advance();
        auto rhs = parseRelational();
        if (!rhs) return std::nullopt;
        lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
      }
      return lhs;
    };

    auto lhs = parseEquality();
    if (!lhs) return std::nullopt;
    while (isAndOp(cur_.kind)) {
      TokenKind op = cur_.kind;
      SourceLocation l = (*lhs)->loc;
      advance();
      auto rhs = parseEquality();
      if (!rhs) return std::nullopt;
      lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
    }
    return lhs;
  };

  auto lhs = parseLogicalAnd();
  if (!lhs) return std::nullopt;

  while (isOrOp(cur_.kind)) {
    TokenKind op = cur_.kind;
    SourceLocation l = (*lhs)->loc;
    advance();
    auto rhs = parseLogicalAnd();
    if (!rhs) return std::nullopt;
    lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
  }

  // assignment := logical_or ( '=' assignment )?
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
    auto rhs = parseExpr(); // right associative
    if (!rhs) return std::nullopt;

    return std::make_unique<AssignExpr>(assignLoc, std::move(name), nameLoc, std::move(*rhs));
  }

  return lhs;
}

// Old precedence helpers kept for header compatibility (unused here)
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

} // namespace c99cc
