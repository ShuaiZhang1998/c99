#include "lexer.h"
#include <cctype>

namespace c99cc {

Lexer::Lexer(const std::string& input, Diagnostics& diags)
  : input_(input), diags_(diags) {}

char Lexer::peek() const {
  if (i_ >= input_.size()) return '\0';
  return input_[i_];
}

char Lexer::get() {
  if (i_ >= input_.size()) return '\0';
  char c = input_[i_++];
  if (c == '\n') { line_++; col_ = 1; }
  else { col_++; }
  return c;
}

bool Lexer::eof() const { return i_ >= input_.size(); }

void Lexer::skipWhitespace() {
  while (!eof()) {
    char c = peek();
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { get(); continue; }

    // support // comments
    if (c == '/' && i_ + 1 < input_.size() && input_[i_ + 1] == '/') {
      while (!eof() && peek() != '\n') get();
      continue;
    }
    break;
  }
}

Token Lexer::lexIdentifierOrKeyword() {
  SourceLocation loc{ i_, line_, col_ };
  std::string s;
  while (!eof()) {
    char c = peek();
    if (std::isalnum((unsigned char)c) || c == '_') s.push_back(get());
    else break;
  }

  if (s == "int")    return Token{TokenKind::KwInt, s, loc};
  if (s == "return") return Token{TokenKind::KwReturn, s, loc};
  return Token{TokenKind::Identifier, s, loc};
}

Token Lexer::lexInteger() {
  SourceLocation loc{ i_, line_, col_ };
  std::string s;
  while (!eof()) {
    char c = peek();
    if (std::isdigit((unsigned char)c)) s.push_back(get());
    else break;
  }
  return Token{TokenKind::IntegerLiteral, s, loc};
}

Token Lexer::next() {
  skipWhitespace();
  SourceLocation loc{ i_, line_, col_ };

  if (eof()) return Token{TokenKind::Eof, "", loc};

  char c = peek();
  if (std::isalpha((unsigned char)c) || c == '_') return lexIdentifierOrKeyword();
  if (std::isdigit((unsigned char)c)) return lexInteger();

  switch (c) {
    case '(': get(); return Token{TokenKind::LParen, "(", loc};
    case ')': get(); return Token{TokenKind::RParen, ")", loc};
    case '{': get(); return Token{TokenKind::LBrace, "{", loc};
    case '}': get(); return Token{TokenKind::RBrace, "}", loc};
    case ';': get(); return Token{TokenKind::Semicolon, ";", loc};

    case '+': get(); return Token{TokenKind::Plus, "+", loc};
    case '-': get(); return Token{TokenKind::Minus, "-", loc};
    case '*': get(); return Token{TokenKind::Star, "*", loc};
    case '/': get(); return Token{TokenKind::Slash, "/", loc};
    case '=': get(); return Token{TokenKind::Assign, "=", loc};

    case '!': get(); return Token{TokenKind::Bang, "!", loc};
    case '~': get(); return Token{TokenKind::Tilde, "~", loc};

    default:
      diags_.error(loc, std::string("unexpected character: '") + c + "'");
      get();
      return next();
  }
}

} // namespace c99cc
