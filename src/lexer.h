#pragma once
#include <string>
#include "diag.h"

namespace c99cc {

enum class TokenKind {
  Eof,
  Identifier,
  IntegerLiteral,

  KwInt,
  KwReturn,
  KwIf,
  KwElse,
  KwWhile,
  KwDo,
  KwFor,
  KwBreak,
  KwContinue,

  LParen, RParen,
  LBrace, RBrace,
  Semicolon,

  Plus, Minus, Star, Slash,
  Assign,

  Comma,

  Bang,   // !
  Tilde,  // ~
  Less,        // <
  Greater,     // >
  LessEqual,   // <=
  GreaterEqual,// >=
  EqualEqual,  // ==
  BangEqual,   // !=

  AmpAmp,   // &&
  PipePipe, // ||
};

struct Token {
  TokenKind kind;
  std::string text;
  SourceLocation loc;
};

class Lexer {
public:
  Lexer(const std::string& input, Diagnostics& diags);
  Token next();

private:
  char peek() const;
  char get();
  bool eof() const;

  void skipWhitespace();
  Token lexIdentifierOrKeyword();
  Token lexInteger();

  const std::string& input_;
  Diagnostics& diags_;
  size_t i_ = 0;
  int line_ = 1;
  int col_ = 1;
};

} // namespace c99cc
