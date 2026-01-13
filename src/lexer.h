#pragma once
#include <string>
#include "diag.h"

namespace c99cc {

enum class TokenKind {
  Eof,
  Identifier,
  IntegerLiteral,

  KwInt,
  KwVoid,
  KwStruct,
  KwReturn,
  KwIf,
  KwElse,
  KwWhile,
  KwDo,
  KwFor,
  KwBreak,
  KwContinue,
  KwSwitch,
  KwCase,
  KwDefault,

  LParen, RParen,
  LBrace, RBrace,
  LBracket, RBracket,
  Semicolon,
  Colon,

  Plus, Minus, Star, Slash,
  Assign,
  Amp,    // &

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

  Question, // ?

  Dot,      // .
  Arrow,    // ->
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
