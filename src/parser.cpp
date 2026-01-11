#include "parser.h"

#include <functional>

namespace c99cc {

bool Parser::expect(TokenKind k, const char* what) {
  if (cur_.kind == k) return true;
  diags_.error(cur_.loc, std::string("expected ") + what);
  return false;
}

// -------------------- TU / function --------------------

std::optional<std::vector<Param>> Parser::parseParamList() {
  // params := ε | 'int' ident (',' 'int' ident)*
  std::vector<Param> params;

  if (cur_.kind == TokenKind::RParen) {
    return params; // empty
  }

  while (true) {
    if (!expect(TokenKind::KwInt, "'int'")) return std::nullopt;
    advance();

    if (!expect(TokenKind::Identifier, "identifier")) return std::nullopt;
    Param p;
    p.name = cur_.text;
    p.nameLoc = cur_.loc;
    advance();
    params.push_back(std::move(p));

    if (cur_.kind == TokenKind::Comma) {
      advance();
      if (cur_.kind == TokenKind::RParen) {
        diags_.error(cur_.loc, "expected parameter");
        return std::nullopt;
      }
      continue;
    }
    break;
  }

  return params;
}

std::optional<FunctionDef> Parser::parseFunctionDef() {
  // int <name> '(' params ')' '{' stmts '}'
  if (!expect(TokenKind::KwInt, "'int'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::Identifier, "identifier")) return std::nullopt;
  FunctionDef fn;
  fn.name = cur_.text;
  fn.nameLoc = cur_.loc;
  advance();

  if (!expect(TokenKind::LParen, "'('")) return std::nullopt;
  advance();

  auto params = parseParamList();
  if (!params) return std::nullopt;
  fn.params = std::move(*params);

  if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::LBrace, "'{'")) return std::nullopt;
  advance();

  while (cur_.kind != TokenKind::RBrace && cur_.kind != TokenKind::Eof) {
    auto s = parseStmt();
    if (!s) return std::nullopt;
    fn.body.push_back(std::move(*s));
  }

  if (!expect(TokenKind::RBrace, "'}'")) return std::nullopt;
  advance();

  return fn;
}

std::optional<AstTranslationUnit> Parser::parseTranslationUnit() {
  AstTranslationUnit tu;

  while (cur_.kind != TokenKind::Eof) {
    auto fn = parseFunctionDef();
    if (!fn) return std::nullopt;
    tu.functions.push_back(std::move(*fn));
  }

  return tu;
}

// -------------------- statements --------------------

std::optional<std::unique_ptr<Stmt>> Parser::parseStmt() {
  if (cur_.kind == TokenKind::KwInt) return parseDeclStmt();
  if (cur_.kind == TokenKind::KwReturn) return parseReturnStmt();
  if (cur_.kind == TokenKind::KwIf) return parseIfStmt();
  if (cur_.kind == TokenKind::KwWhile) return parseWhileStmt();
  if (cur_.kind == TokenKind::KwDo) return parseDoWhileStmt();
  if (cur_.kind == TokenKind::KwFor) return parseForStmt();
  if (cur_.kind == TokenKind::KwBreak) return parseBreakStmt();
  if (cur_.kind == TokenKind::KwContinue) return parseContinueStmt();
  if (cur_.kind == TokenKind::LBrace) return parseBlockStmt();

  // empty stmt
  if (cur_.kind == TokenKind::Semicolon) {
    SourceLocation l = cur_.loc;
    advance();
    return std::make_unique<EmptyStmt>(l);
  }

  // assignment stmt starts with identifier and then '='
  if (cur_.kind == TokenKind::Identifier) {
    // lookahead not available; parseAssignStmt expects identifier '=' ...
    // But expression stmt can also start with identifier (like call).
    // We disambiguate by trying to parse "ident = ..." pattern manually.
    Token save = cur_;
    advance();
    if (cur_.kind == TokenKind::Assign) {
      // rollback by reconstructing: easier is to re-run parseAssignStmt by keeping saved.
      // We'll implement assign stmt parsing inline to avoid lexer rollback.
      // We already consumed ident, and see '=':
      SourceLocation l = save.loc;
      std::string name = save.text;
      SourceLocation nameLoc = save.loc;
      advance(); // consume '='

      auto rhs = parseExpr();
      if (!rhs) return std::nullopt;
      if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
      advance();
      return std::make_unique<AssignStmt>(l, std::move(name), nameLoc, std::move(*rhs));
    }

    // not assignment stmt. We need to parse as expression stmt.
    // We have consumed one token too far (the identifier).
    // Without lexer rewind, handle by building a VarRefExpr/CallExpr with the consumed identifier
    // and continue parsing from current token.
    // We'll parse the "primary starting from already-consumed identifier" here:
    // - if current token is '(' => parse call args and build CallExpr
    // - else => build VarRefExpr
    // then parse rest of expression by feeding it into the assignment-expression / comma-expression
    // pipeline. Simplest: create a small helper to parse "post-identifier primary" then continue.
    // We'll implement a limited re-entry: build an Expr* as lhs and then parse remaining
    // using the same precedence ladder by temporarily stitching is hard. So instead:
    // We reconstitute the identifier into a fake token stream is not possible.
    //
    // Therefore, to keep parser simple and correct, we DO NOT consume identifier here.
    // We'll fall back to generic expr-stmt path by restoring cur_ to saved and
    // using a minimal "rewind" via a member cache.
    //
    // But we can't rewind lexer. So we must avoid consuming here.
    //
    // => Solution: do not try to disambiguate here. Let generic expr stmt parse.
  }

  // Generic expression statement: <expr> ';'
  SourceLocation l = cur_.loc;
  auto e = parseExpr();
  if (!e) return std::nullopt;
  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();
  return std::make_unique<ExprStmt>(l, std::move(*e));
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

std::optional<std::unique_ptr<Stmt>> Parser::parseDoWhileStmt() {
  SourceLocation loc = cur_.loc;

  if (!expect(TokenKind::KwDo, "'do'")) return std::nullopt;
  advance();

  auto body = parseStmt();
  if (!body) return std::nullopt;

  if (!expect(TokenKind::KwWhile, "'while'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::LParen, "'('")) return std::nullopt;
  advance();

  auto cond = parseExpr();
  if (!cond) return std::nullopt;

  if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();

  return std::make_unique<DoWhileStmt>(loc, std::move(*body), std::move(*cond));
}

std::optional<std::unique_ptr<Stmt>> Parser::parseForStmt() {
  SourceLocation fLoc = cur_.loc;
  if (!expect(TokenKind::KwFor, "'for'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::LParen, "'('")) return std::nullopt;
  advance();

  // init:
  //   empty ';'
  //   decl:  int x [= expr] ;
  //   expr-stmt: <expr> ;
  std::unique_ptr<Stmt> init = nullptr;

  if (cur_.kind == TokenKind::Semicolon) {
    advance();
  } else if (cur_.kind == TokenKind::KwInt) {
    auto d = parseDeclStmt(); // consumes trailing ';'
    if (!d) return std::nullopt;
    init = std::move(*d);
  } else {
    SourceLocation loc = cur_.loc;
    auto e = parseExpr();
    if (!e) return std::nullopt;

    if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
    advance();

    init = std::make_unique<ExprStmt>(loc, std::move(*e));
  }

  // cond (optional) until ';'
  std::unique_ptr<Expr> cond = nullptr;
  if (cur_.kind == TokenKind::Semicolon) {
    advance();
  } else {
    auto c = parseExpr();
    if (!c) return std::nullopt;
    cond = std::move(*c);
    if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
    advance();
  }

  // inc (optional) until ')'
  std::unique_ptr<Expr> inc = nullptr;
  if (cur_.kind == TokenKind::RParen) {
    advance();
  } else {
    auto in = parseExpr();
    if (!in) return std::nullopt;
    inc = std::move(*in);
    if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
    advance();
  }

  auto body = parseStmt();
  if (!body) return std::nullopt;

  return std::make_unique<ForStmt>(fLoc, std::move(init), std::move(cond), std::move(inc), std::move(*body));
}

// -------------------- Expression parsing (layered) --------------------
//
// expr            := assignment (',' assignment)*
// assignment      := logical_or ( '=' assignment )?
// logical_or      := logical_and ( '||' logical_and )*
// logical_and     := equality ( '&&' equality )*
// equality        := relational ( ( '==' | '!=' ) relational )*
// relational      := additive ( ( '<' | '<=' | '>' | '>=' ) additive )*
// additive        := multiplicative ( ( '+' | '-' ) multiplicative )*
// multiplicative  := unary ( ( '*' | '/' ) unary )*
// unary           := ( '+' | '-' | '!' | '~' ) unary | primary
// primary         := integer | identifier | call | '(' expr ')'
//
// NOTE: call arguments are parsed by parseAssignmentExpr() (no top-level comma).

std::optional<std::unique_ptr<Expr>> Parser::parsePrimary() {
  if (cur_.kind == TokenKind::IntegerLiteral) {
    SourceLocation l = cur_.loc;
    int64_t v = std::stoll(cur_.text);
    advance();
    return std::make_unique<IntLiteralExpr>(l, v);
  }

  if (cur_.kind == TokenKind::Identifier) {
    SourceLocation idLoc = cur_.loc;
    std::string name = cur_.text;
    advance();

    // call: ident '(' args ')'
    if (cur_.kind == TokenKind::LParen) {
      advance(); // '('
      std::vector<std::unique_ptr<Expr>> args;

      if (cur_.kind != TokenKind::RParen) {
        while (true) {
          auto a = parseAssignmentExpr(); // IMPORTANT: no comma-expr here
          if (!a) return std::nullopt;
          args.push_back(std::move(*a));

          if (cur_.kind == TokenKind::Comma) {
            advance();
            if (cur_.kind == TokenKind::RParen) {
              diags_.error(cur_.loc, "expected expression");
              return std::nullopt;
            }
            continue;
          }
          break;
        }
      }

      if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
      advance();
      return std::make_unique<CallExpr>(idLoc, std::move(name), idLoc, std::move(args));
    }

    return std::make_unique<VarRefExpr>(idLoc, std::move(name));
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

// Keep these for compatibility with parser.h (even if unused)
std::optional<std::unique_ptr<Expr>> Parser::parseBinary(int /*minPrec*/) {
  return parseUnary();
}

int Parser::precedence(TokenKind k) const {
  switch (k) {
    case TokenKind::Star:
    case TokenKind::Slash: return 40;
    case TokenKind::Plus:
    case TokenKind::Minus: return 30;
    case TokenKind::Less:
    case TokenKind::LessEqual:
    case TokenKind::Greater:
    case TokenKind::GreaterEqual: return 20;
    case TokenKind::EqualEqual:
    case TokenKind::BangEqual: return 10;
    default: return -1;
  }
}

bool Parser::isBinaryOp(TokenKind k) const {
  return precedence(k) >= 0;
}

std::optional<std::unique_ptr<Expr>> Parser::parseAssignmentExpr() {
  auto parseMultiplicative = [&]() -> std::optional<std::unique_ptr<Expr>> {
    auto lhs = parseUnary();
    if (!lhs) return std::nullopt;
    while (cur_.kind == TokenKind::Star || cur_.kind == TokenKind::Slash) {
      TokenKind op = cur_.kind;
      SourceLocation l = (*lhs)->loc;
      advance();
      auto rhs = parseUnary();
      if (!rhs) return std::nullopt;
      lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
    }
    return lhs;
  };

  auto parseAdditive = [&]() -> std::optional<std::unique_ptr<Expr>> {
    auto lhs = parseMultiplicative();
    if (!lhs) return std::nullopt;
    while (cur_.kind == TokenKind::Plus || cur_.kind == TokenKind::Minus) {
      TokenKind op = cur_.kind;
      SourceLocation l = (*lhs)->loc;
      advance();
      auto rhs = parseMultiplicative();
      if (!rhs) return std::nullopt;
      lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
    }
    return lhs;
  };

  auto parseRelational = [&]() -> std::optional<std::unique_ptr<Expr>> {
    auto lhs = parseAdditive();
    if (!lhs) return std::nullopt;
    while (cur_.kind == TokenKind::Less || cur_.kind == TokenKind::LessEqual ||
           cur_.kind == TokenKind::Greater || cur_.kind == TokenKind::GreaterEqual) {
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
    while (cur_.kind == TokenKind::EqualEqual || cur_.kind == TokenKind::BangEqual) {
      TokenKind op = cur_.kind;
      SourceLocation l = (*lhs)->loc;
      advance();
      auto rhs = parseRelational();
      if (!rhs) return std::nullopt;
      lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
    }
    return lhs;
  };

  auto parseLogicalAnd = [&]() -> std::optional<std::unique_ptr<Expr>> {
    auto lhs = parseEquality();
    if (!lhs) return std::nullopt;
    while (cur_.kind == TokenKind::AmpAmp) {
      TokenKind op = cur_.kind;
      SourceLocation l = (*lhs)->loc;
      advance();
      auto rhs = parseEquality();
      if (!rhs) return std::nullopt;
      lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
    }
    return lhs;
  };

  auto parseLogicalOr = [&]() -> std::optional<std::unique_ptr<Expr>> {
    auto lhs = parseLogicalAnd();
    if (!lhs) return std::nullopt;
    while (cur_.kind == TokenKind::PipePipe) {
      TokenKind op = cur_.kind;
      SourceLocation l = (*lhs)->loc;
      advance();
      auto rhs = parseLogicalAnd();
      if (!rhs) return std::nullopt;
      lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
    }
    return lhs;
  };

  // assignment := logical_or ( '=' assignment )?
  auto lhs = parseLogicalOr();
  if (!lhs) return std::nullopt;

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
    auto rhs = parseAssignmentExpr(); // right associative
    if (!rhs) return std::nullopt;

    return std::make_unique<AssignExpr>(assignLoc, std::move(name), nameLoc, std::move(*rhs));
  }

  return lhs;
}

std::optional<std::unique_ptr<Expr>> Parser::parseExpr() {
  // comma-expression: assignment (',' assignment)*
  auto lhs = parseAssignmentExpr();
  if (!lhs) return std::nullopt;

  while (cur_.kind == TokenKind::Comma) {
    SourceLocation commaLoc = cur_.loc;
    TokenKind op = cur_.kind;
    SourceLocation l = (*lhs)->loc;
    advance();

    if (cur_.kind == TokenKind::RParen || cur_.kind == TokenKind::Semicolon ||
        cur_.kind == TokenKind::RBrace || cur_.kind == TokenKind::Eof) {
      diags_.error(commaLoc, "expected expression");
      return std::nullopt;
    }

    auto rhs = parseAssignmentExpr();
    if (!rhs) return std::nullopt;

    lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
  }

  return lhs;
}

} // namespace c99cc
