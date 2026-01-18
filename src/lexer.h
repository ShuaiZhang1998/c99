#pragma once
#include <optional>
#include <string>
#include "diag.h"

namespace c99cc {

enum class TokenKind {
  Eof,
  Identifier,
  IntegerLiteral,
  FloatLiteral,
  CharLiteral,
  StringLiteral,

  KwChar,
  KwShort,
  KwInt,
  KwLong,
  KwUnsigned,
  KwFloat,
  KwDouble,
  KwVoid,
  KwStruct,
  KwEnum,
  KwTypedef,
  KwSizeof,
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
  KwConst,

  LParen, RParen,
  LBrace, RBrace,
  LBracket, RBracket,
  Semicolon,
  Colon,

  Plus, Minus, Star, Slash,
  Percent,
  Assign,
  Amp,    // &
  Pipe,   // |
  Caret,  // ^

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

  PlusPlus,  // ++
  MinusMinus, // --

  PlusAssign,     // +=
  MinusAssign,    // -=
  StarAssign,     // *=
  SlashAssign,    // /=
  PercentAssign,  // %=
  AmpAssign,      // &=
  PipeAssign,     // |=
  CaretAssign,    // ^=
  LessLess,       // <<
  GreaterGreater, // >>
  LessLessAssign, // <<=
  GreaterGreaterAssign // >>=
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
  Token lexNumber();
  Token lexStringLiteral();
  Token lexCharLiteral();
  std::optional<char> parseEscapeChar(SourceLocation loc);

  const std::string& input_;
  Diagnostics& diags_;
  size_t i_ = 0;
  int line_ = 1;
  int col_ = 1;
};

} // namespace c99cc
