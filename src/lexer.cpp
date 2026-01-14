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

  if (s == "char")     return Token{TokenKind::KwChar, s, loc};
  if (s == "short")    return Token{TokenKind::KwShort, s, loc};
  if (s == "int")      return Token{TokenKind::KwInt, s, loc};
  if (s == "long")     return Token{TokenKind::KwLong, s, loc};
  if (s == "unsigned") return Token{TokenKind::KwUnsigned, s, loc};
  if (s == "float")    return Token{TokenKind::KwFloat, s, loc};
  if (s == "double")   return Token{TokenKind::KwDouble, s, loc};
  if (s == "void")     return Token{TokenKind::KwVoid, s, loc};
  if (s == "struct")   return Token{TokenKind::KwStruct, s, loc};
  if (s == "return")   return Token{TokenKind::KwReturn, s, loc};
  if (s == "if")       return Token{TokenKind::KwIf, s, loc};
  if (s == "else")     return Token{TokenKind::KwElse, s, loc};
  if (s == "while")    return Token{TokenKind::KwWhile, s, loc};
  if (s == "for")      return Token{TokenKind::KwFor, s, loc};
  if (s == "break")    return Token{TokenKind::KwBreak, s, loc};
  if (s == "continue") return Token{TokenKind::KwContinue, s, loc};
  if (s == "do") return Token{TokenKind::KwDo, s, loc};
  if (s == "switch")   return Token{TokenKind::KwSwitch, s, loc};
  if (s == "case")     return Token{TokenKind::KwCase, s, loc};
  if (s == "default")  return Token{TokenKind::KwDefault, s, loc};
  if (s == "NULL")     return Token{TokenKind::IntegerLiteral, "0", loc};

  return Token{TokenKind::Identifier, s, loc};
}

Token Lexer::lexNumber() {
  SourceLocation loc{ i_, line_, col_ };
  std::string s;
  bool isFloat = false;
  if (peek() == '.') {
    isFloat = true;
    s.push_back(get());
    while (!eof()) {
      char c = peek();
      if (std::isdigit((unsigned char)c)) s.push_back(get());
      else break;
    }
  } else {
    while (!eof()) {
      char c = peek();
      if (std::isdigit((unsigned char)c)) s.push_back(get());
      else break;
    }
    if (!eof() && peek() == '.') {
      isFloat = true;
      s.push_back(get());
      while (!eof()) {
        char c = peek();
        if (std::isdigit((unsigned char)c)) s.push_back(get());
        else break;
      }
    }
  }

  if (!eof() && (peek() == 'e' || peek() == 'E')) {
    isFloat = true;
    s.push_back(get());
    if (!eof() && (peek() == '+' || peek() == '-')) s.push_back(get());
    while (!eof()) {
      char c = peek();
      if (std::isdigit((unsigned char)c)) s.push_back(get());
      else break;
    }
  }

  if (!eof() && (peek() == 'f' || peek() == 'F')) {
    isFloat = true;
    s.push_back(get());
  }

  if (isFloat) return Token{TokenKind::FloatLiteral, s, loc};
  return Token{TokenKind::IntegerLiteral, s, loc};
}

Token Lexer::next() {
  skipWhitespace();
  SourceLocation loc{ i_, line_, col_ };

  if (eof()) return Token{TokenKind::Eof, "", loc};

  char c = peek();
  if (std::isalpha((unsigned char)c) || c == '_') return lexIdentifierOrKeyword();
  if (std::isdigit((unsigned char)c)) return lexNumber();
  if (c == '.' && i_ + 1 < input_.size() && std::isdigit((unsigned char)input_[i_ + 1])) {
    return lexNumber();
  }

  switch (c) {
    case '(': get(); return Token{TokenKind::LParen, "(", loc};
    case ')': get(); return Token{TokenKind::RParen, ")", loc};
    case '{': get(); return Token{TokenKind::LBrace, "{", loc};
    case '}': get(); return Token{TokenKind::RBrace, "}", loc};
    case '[': get(); return Token{TokenKind::LBracket, "[", loc};
    case ']': get(); return Token{TokenKind::RBracket, "]", loc};
    case ';': get(); return Token{TokenKind::Semicolon, ";", loc};
    case ':': get(); return Token{TokenKind::Colon, ":", loc};

    case '+': get(); return Token{TokenKind::Plus, "+", loc};
    case '-': {
      get();
      if (!eof() && peek() == '>') { get(); return Token{TokenKind::Arrow, "->", loc}; }
      return Token{TokenKind::Minus, "-", loc};
    }
    case '*': get(); return Token{TokenKind::Star, "*", loc};
    case '/': get(); return Token{TokenKind::Slash, "/", loc};

    case '&': {
      get();
      if (!eof() && peek() == '&') { get(); return Token{TokenKind::AmpAmp, "&&", loc}; }
      return Token{TokenKind::Amp, "&", loc};
    }

    case '|': {
      get();
      if (!eof() && peek() == '|') { get(); return Token{TokenKind::PipePipe, "||", loc}; }
      diags_.error(loc, "unexpected character: '|'");
      return next();
    }

    case '=': {
      get();
      if (!eof() && peek() == '=') { get(); return Token{TokenKind::EqualEqual, "==", loc}; }
      return Token{TokenKind::Assign, "=", loc};
    }

    case '<': {
      get();
      if (!eof() && peek() == '=') { get(); return Token{TokenKind::LessEqual, "<=", loc}; }
      return Token{TokenKind::Less, "<", loc};
    }

    case '>': {
      get();
      if (!eof() && peek() == '=') { get(); return Token{TokenKind::GreaterEqual, ">=", loc}; }
      return Token{TokenKind::Greater, ">", loc};
    }

    case '!': {
      get();
      if (!eof() && peek() == '=') { get(); return Token{TokenKind::BangEqual, "!=", loc}; }
      return Token{TokenKind::Bang, "!", loc};
    }

    case '~': get(); return Token{TokenKind::Tilde, "~", loc};

    case '?': get(); return Token{TokenKind::Question, "?", loc};

    case ',': get(); return Token{TokenKind::Comma, ",", loc};
    case '.': get(); return Token{TokenKind::Dot, ".", loc};

    default:
      diags_.error(loc, std::string("unexpected character: '") + c + "'");
      get();
      return next();
  }
}

} // namespace c99cc
