#include "preprocessor.h"

#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>

namespace c99cc {

Preprocessor::Preprocessor(
    std::vector<std::string> includePaths,
    std::vector<std::string> systemIncludePaths)
    : includePaths_(std::move(includePaths)),
      systemIncludePaths_(std::move(systemIncludePaths)) {
  std::time_t now = std::time(nullptr);
  std::tm tm = *std::localtime(&now);
  {
    std::ostringstream oss;
    oss << std::put_time(&tm, "%b %e %Y");
    builtinDate_ = oss.str();
  }
  {
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    builtinTime_ = oss.str();
  }
}

void Preprocessor::addIncludePath(const std::string& path) {
  includePaths_.push_back(path);
}

void Preprocessor::addSystemIncludePath(const std::string& path) {
  systemIncludePaths_.push_back(path);
}

static bool isIdentStart(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

static bool isIdentChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

static std::string trim(const std::string& s) {
  size_t b = 0;
  while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) b++;
  size_t e = s.size();
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) e--;
  return s.substr(b, e - b);
}

static std::string ltrim(const std::string& s) {
  size_t i = 0;
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
  return s.substr(i);
}

static std::string rtrim(const std::string& s) {
  if (s.empty()) return s;
  size_t i = s.size();
  while (i > 0 && std::isspace(static_cast<unsigned char>(s[i - 1]))) i--;
  return s.substr(0, i);
}

static std::string stringize(const std::string& raw) {
  std::string out = "\"";
  for (char c : raw) {
    if (c == '\\' || c == '"') out.push_back('\\');
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

static std::string toStringLiteral(const std::string& raw) {
  std::string out = "\"";
  for (char c : raw) {
    if (c == '\\' || c == '"') out.push_back('\\');
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

static std::string replaceParams(
    const std::string& body,
    const std::vector<std::string>& params,
    const std::vector<std::string>& argsRaw,
    const std::vector<std::string>& argsExpanded,
    bool variadic,
    const std::string& varRaw,
    const std::string& varExpanded) {
  std::unordered_map<std::string, std::string> mapExpanded;
  std::unordered_map<std::string, std::string> mapRaw;
  for (size_t i = 0; i < params.size() && i < argsExpanded.size(); ++i) {
    mapExpanded.emplace(params[i], argsExpanded[i]);
    mapRaw.emplace(params[i], argsRaw[i]);
  }
  if (variadic) {
    mapExpanded.emplace("__VA_ARGS__", varExpanded);
    mapRaw.emplace("__VA_ARGS__", varRaw);
  }

  std::string out;
  size_t i = 0;
  bool pendingPaste = false;
  while (i < body.size()) {
    char c = body[i];
    if (c == '"' || c == '\'') {
      char quote = c;
      std::string lit;
      lit.push_back(c);
      i++;
      while (i < body.size()) {
        char cc = body[i];
        lit.push_back(cc);
        i++;
        if (cc == '\\' && i < body.size()) {
          lit.push_back(body[i]);
          i++;
          continue;
        }
        if (cc == quote) break;
      }
      if (pendingPaste) {
        lit = ltrim(lit);
        out += lit;
        pendingPaste = false;
      } else {
        out += lit;
      }
      continue;
    }
    if (c == '#') {
      if (i + 1 < body.size() && body[i + 1] == '#') {
        out = rtrim(out);
        pendingPaste = true;
        i += 2;
        continue;
      }
      size_t j = i + 1;
      while (j < body.size() && std::isspace(static_cast<unsigned char>(body[j]))) j++;
      if (j < body.size() && isIdentStart(body[j])) {
        size_t start = j;
        j++;
        while (j < body.size() && isIdentChar(body[j])) j++;
        std::string name = body.substr(start, j - start);
        auto it = mapRaw.find(name);
        std::string rep;
        if (it != mapRaw.end()) rep = stringize(it->second);
        else rep = "#" + name;
        if (pendingPaste) {
          rep = ltrim(rep);
          out += rep;
          pendingPaste = false;
        } else {
          out += rep;
        }
        i = j;
        continue;
      }
      out.push_back(c);
      i++;
      continue;
    }
    if (isIdentStart(c)) {
      size_t start = i;
      i++;
      while (i < body.size() && isIdentChar(body[i])) i++;
      std::string name = body.substr(start, i - start);
      auto it = mapExpanded.find(name);
      std::string rep = (it != mapExpanded.end()) ? it->second : name;
      if (pendingPaste) {
        rep = ltrim(rep);
        if (!rep.empty()) out += rep;
        pendingPaste = false;
      } else {
        out += rep;
      }
      continue;
    }
    if (pendingPaste && std::isspace(static_cast<unsigned char>(c))) {
      i++;
      continue;
    }
    if (pendingPaste) {
      out.push_back(c);
      pendingPaste = false;
      i++;
      continue;
    }
    out.push_back(c);
    i++;
  }
  return out;
}

std::optional<std::string> Preprocessor::run(const std::string& path, const std::string& source) {
  errors_.clear();
  std::string out;
  if (!processFile(path, source, out)) return std::nullopt;
  return out;
}

bool Preprocessor::processFile(const std::string& path, const std::string& source, std::string& out) {
  return processLines(path, source, out);
}

bool Preprocessor::processLines(const std::string& path, const std::string& source, std::string& out) {
  std::istringstream iss(source);
  std::string line;
  int lineNo = 1;
  std::vector<IfState> ifs;

  while (std::getline(iss, line)) {
    size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) i++;
    if (i < line.size() && line[i] == '#') {
      if (!handleDirective(path, lineNo, line.substr(i + 1), ifs, out)) return false;
      lineNo++;
      continue;
    }

    bool curActive = true;
    if (!ifs.empty()) {
      const IfState& st = ifs.back();
      curActive = st.parentActive && st.condition;
    }

    if (curActive) {
      out.append(expandLine(line, path, lineNo));
      out.push_back('\n');
    }
    lineNo++;
  }

  if (!ifs.empty()) {
    report(path, lineNo, 1, "unterminated conditional directive");
    return false;
  }

  return true;
}

bool Preprocessor::handleDirective(
    const std::string& path, int line, const std::string& lineText,
    std::vector<IfState>& ifs, std::string& out) {
  size_t i = 0;
  while (i < lineText.size() && std::isspace(static_cast<unsigned char>(lineText[i]))) i++;
  size_t start = i;
  while (i < lineText.size() && isIdentChar(lineText[i])) i++;
  std::string directive = lineText.substr(start, i - start);
  while (i < lineText.size() && std::isspace(static_cast<unsigned char>(lineText[i]))) i++;

  bool active = true;
  if (!ifs.empty()) {
    const IfState& st = ifs.back();
    active = st.parentActive && st.condition;
  }

  if (directive == "include") {
    if (!active) return true;
    if (i >= lineText.size()) return report(path, line, static_cast<int>(i + 1), "expected header");
    char delim = lineText[i];
    if (delim != '"' && delim != '<') {
      return report(path, line, static_cast<int>(i + 1), "expected '\"' or '<' after include");
    }
    ++i;
    size_t nameStart = i;
    while (i < lineText.size() && lineText[i] != (delim == '"' ? '"' : '>')) i++;
    if (i >= lineText.size()) {
      return report(path, line, static_cast<int>(nameStart + 1), "unterminated include path");
    }
    std::string header = lineText.substr(nameStart, i - nameStart);
    std::string fullPath;
    std::string content;
    if (!resolveInclude(header, delim == '<', path, fullPath, content)) {
      return report(path, line, static_cast<int>(nameStart + 1),
                    "include file not found: " + header);
    }
    if (!processFile(fullPath, content, out)) return false;
    return true;
  }

  if (directive == "define") {
    if (!active) return true;
    if (i >= lineText.size() || !isIdentStart(lineText[i])) {
      return report(path, line, static_cast<int>(i + 1), "expected macro name");
    }
    size_t nameStart = i;
    while (i < lineText.size() && isIdentChar(lineText[i])) i++;
    std::string name = lineText.substr(nameStart, i - nameStart);
    Macro macro;
    if (i < lineText.size() && lineText[i] == '(') {
      // function-like macro (no whitespace allowed before '(')
      macro.functionLike = true;
      i++; // '('
      while (i < lineText.size()) {
        while (i < lineText.size() && std::isspace(static_cast<unsigned char>(lineText[i]))) i++;
        if (i < lineText.size() && lineText[i] == ')') {
          i++;
          break;
        }
        if (i + 2 < lineText.size() &&
            lineText[i] == '.' && lineText[i + 1] == '.' && lineText[i + 2] == '.') {
          macro.variadic = true;
          i += 3;
          while (i < lineText.size() && std::isspace(static_cast<unsigned char>(lineText[i]))) i++;
          if (i < lineText.size() && lineText[i] == ')') {
            i++;
            break;
          }
          if (i >= lineText.size()) {
            return report(path, line, static_cast<int>(i + 1), "unterminated macro parameters");
          }
          return report(path, line, static_cast<int>(i + 1), "expected ')'");
        }
        if (i >= lineText.size() || !isIdentStart(lineText[i])) {
          return report(path, line, static_cast<int>(i + 1), "expected parameter name");
        }
        size_t pStart = i;
        i++;
        while (i < lineText.size() && isIdentChar(lineText[i])) i++;
        macro.params.push_back(lineText.substr(pStart, i - pStart));
        while (i < lineText.size() && std::isspace(static_cast<unsigned char>(lineText[i]))) i++;
        if (i < lineText.size() && lineText[i] == ',') {
          i++;
          continue;
        }
        if (i < lineText.size() && lineText[i] == ')') {
          i++;
          break;
        }
        if (i >= lineText.size()) {
          return report(path, line, static_cast<int>(i + 1), "unterminated macro parameters");
        }
        return report(path, line, static_cast<int>(i + 1), "expected ',' or ')'");
      }
    }
    while (i < lineText.size() && std::isspace(static_cast<unsigned char>(lineText[i]))) i++;
    macro.body = (i < lineText.size()) ? lineText.substr(i) : "";
    macros_[name] = std::move(macro);
    return true;
  }

  if (directive == "undef") {
    if (!active) return true;
    if (i >= lineText.size() || !isIdentStart(lineText[i])) {
      return report(path, line, static_cast<int>(i + 1), "expected macro name");
    }
    size_t nameStart = i;
    while (i < lineText.size() && isIdentChar(lineText[i])) i++;
    std::string name = lineText.substr(nameStart, i - nameStart);
    macros_.erase(name);
    return true;
  }

  if (directive == "ifdef" || directive == "ifndef") {
    if (i >= lineText.size() || !isIdentStart(lineText[i])) {
      return report(path, line, static_cast<int>(i + 1), "expected macro name");
    }
    size_t nameStart = i;
    while (i < lineText.size() && isIdentChar(lineText[i])) i++;
    std::string name = lineText.substr(nameStart, i - nameStart);
    bool defined = macros_.count(name) > 0;
    bool cond = directive == "ifdef" ? defined : !defined;
    bool parentActive = true;
    if (!ifs.empty()) {
      const IfState& st = ifs.back();
      parentActive = st.parentActive && st.condition;
    }
    ifs.push_back(IfState{parentActive, cond, false, cond});
    return true;
  }

  if (directive == "if") {
    std::string expr = lineText.substr(i);
    bool cond = false;
    std::string err;
    bool parentActive = true;
    if (!ifs.empty()) {
      const IfState& st = ifs.back();
      parentActive = st.parentActive && st.condition;
    }
    if (!parentActive) {
      ifs.push_back(IfState{parentActive, false, false, false});
      return true;
    }
    if (!evalIfExpr(expr, cond, err)) {
      return report(path, line, static_cast<int>(i + 1), err);
    }
    ifs.push_back(IfState{parentActive, cond, false, cond});
    return true;
  }

  if (directive == "elif") {
    if (ifs.empty()) return report(path, line, 1, "unexpected #elif");
    IfState& st = ifs.back();
    if (st.inElse) return report(path, line, 1, "unexpected #elif after #else");
    std::string expr = lineText.substr(i);
    bool cond = false;
    std::string err;
    if (st.parentActive && !st.taken) {
      if (!evalIfExpr(expr, cond, err)) {
        return report(path, line, static_cast<int>(i + 1), err);
      }
    } else {
      cond = false;
    }
    st.condition = cond && st.parentActive && !st.taken;
    st.taken = st.taken || st.condition;
    return true;
  }

  if (directive == "else") {
    if (ifs.empty()) return report(path, line, 1, "unexpected #else");
    IfState& st = ifs.back();
    if (st.inElse) return report(path, line, 1, "duplicate #else");
    st.inElse = true;
    st.condition = st.parentActive && !st.taken;
    st.taken = true;
    return true;
  }

  if (directive == "endif") {
    if (ifs.empty()) return report(path, line, 1, "unexpected #endif");
    ifs.pop_back();
    return true;
  }

  if (directive.empty()) {
    return true;
  }

  return report(path, line, static_cast<int>(start + 1), "unknown preprocessor directive");
}

static bool isAbsolutePath(const std::string& path) {
  if (path.empty()) return false;
  if (path[0] == '/') return true;
  if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
    return true;
  }
  return false;
}

bool Preprocessor::resolveInclude(
    const std::string& header,
    bool isSystem,
    const std::string& currentPath,
    std::string& resolvedPath,
    std::string& content) {
  if (isAbsolutePath(header)) {
    if (readFile(header, content)) {
      resolvedPath = header;
      return true;
    }
    return false;
  }

  std::vector<std::string> searchPaths;
  if (!isSystem) {
    std::string base = dirName(currentPath);
    if (!base.empty()) searchPaths.push_back(base);
  }
  for (const auto& p : includePaths_) searchPaths.push_back(p);
  for (const auto& p : systemIncludePaths_) searchPaths.push_back(p);

  for (const auto& base : searchPaths) {
    std::string fullPath = base;
    if (!fullPath.empty() && fullPath.back() != '/') fullPath += "/";
    fullPath += header;
    if (readFile(fullPath, content)) {
      resolvedPath = fullPath;
      return true;
    }
  }
  return false;
}

bool Preprocessor::evalIfExpr(const std::string& expr, bool& out, std::string& err) {
  struct Token {
    enum Kind {
      End,
      Number,
      Ident,
      LParen,
      RParen,
      Op
    } kind = End;
    long long number = 0;
    std::string text;
  };

  struct Lexer {
    const std::string& s;
    size_t i = 0;
    explicit Lexer(const std::string& in) : s(in) {}
    Token next() {
      while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
      if (i >= s.size()) return Token{Token::End, 0, ""};
      char c = s[i];
      if (std::isdigit(static_cast<unsigned char>(c))) {
        size_t start = i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) i++;
        std::string num = s.substr(start, i - start);
        long long v = std::strtoll(num.c_str(), nullptr, 10);
        return Token{Token::Number, v, num};
      }
      if (isIdentStart(c)) {
        size_t start = i;
        i++;
        while (i < s.size() && isIdentChar(s[i])) i++;
        return Token{Token::Ident, 0, s.substr(start, i - start)};
      }
      if (c == '(') { i++; return Token{Token::LParen, 0, "("}; }
      if (c == ')') { i++; return Token{Token::RParen, 0, ")"}; }
      if (i + 1 < s.size()) {
        std::string two = s.substr(i, 2);
        if (two == "&&" || two == "||" || two == "==" || two == "!=" ||
            two == "<=" || two == ">=" || two == "<<" || two == ">>") {
          i += 2;
          return Token{Token::Op, 0, two};
        }
      }
      i++;
      return Token{Token::Op, 0, std::string(1, c)};
    }
  };

  struct Parser {
    Preprocessor& pp;
    Lexer lex;
    Token cur;
    std::string err;
    explicit Parser(Preprocessor& p, const std::string& e) : pp(p), lex(e) { cur = lex.next(); }

    void consume() { cur = lex.next(); }

    bool expect(Token::Kind k, const char* msg) {
      if (cur.kind == k) return true;
      err = msg;
      return false;
    }

    bool parseNumber(const std::string& text, long long& outVal) {
      std::string t = trim(text);
      if (t.empty()) { outVal = 0; return true; }
      char* end = nullptr;
      long long n = std::strtoll(t.c_str(), &end, 10);
      std::string rest = trim(end ? std::string(end) : std::string());
      if (!end || !rest.empty()) {
        err = "invalid #if expression";
        return false;
      }
      outVal = n;
      return true;
    }

    bool parsePrimary(long long& outVal) {
      if (cur.kind == Token::Number) {
        outVal = cur.number;
        consume();
        return true;
      }
      if (cur.kind == Token::Ident) {
        if (cur.text == "defined") {
          consume();
          std::string name;
          if (cur.kind == Token::LParen) {
            consume();
            if (!expect(Token::Ident, "expected macro name in defined()")) return false;
            name = cur.text;
            consume();
            if (!expect(Token::RParen, "unterminated defined()")) return false;
            consume();
          } else {
            if (!expect(Token::Ident, "expected macro name in defined()")) return false;
            name = cur.text;
            consume();
          }
          outVal = pp.macros_.count(name) > 0 ? 1 : 0;
          return true;
        }
        std::string name = cur.text;
        consume();
        auto it = pp.macros_.find(name);
        if (it == pp.macros_.end()) {
          outVal = 0;
          return true;
        }
        if (it->second.functionLike) {
          outVal = 0;
          return true;
        }
        long long v = 0;
        if (!parseNumber(it->second.body, v)) return false;
        outVal = v;
        return true;
      }
      if (cur.kind == Token::LParen) {
        consume();
        if (!parseExpr(outVal)) return false;
        if (!expect(Token::RParen, "expected ')'")) return false;
        consume();
        return true;
      }
      err = "invalid #if expression";
      return false;
    }

    bool parseUnary(long long& outVal) {
      if (cur.kind == Token::Op &&
          (cur.text == "!" || cur.text == "+" || cur.text == "-" || cur.text == "~")) {
        std::string op = cur.text;
        consume();
        if (!parseUnary(outVal)) return false;
        if (op == "!") outVal = outVal ? 0 : 1;
        else if (op == "-") outVal = -outVal;
        else if (op == "~") outVal = ~outVal;
        return true;
      }
      return parsePrimary(outVal);
    }

    bool parseMul(long long& outVal) {
      if (!parseUnary(outVal)) return false;
      while (cur.kind == Token::Op && (cur.text == "*" || cur.text == "/" || cur.text == "%")) {
        std::string op = cur.text;
        consume();
        long long rhs = 0;
        if (!parseUnary(rhs)) return false;
        if (op == "*") outVal = outVal * rhs;
        else if (op == "/") outVal = rhs == 0 ? 0 : outVal / rhs;
        else outVal = rhs == 0 ? 0 : outVal % rhs;
      }
      return true;
    }

    bool parseAdd(long long& outVal) {
      if (!parseMul(outVal)) return false;
      while (cur.kind == Token::Op && (cur.text == "+" || cur.text == "-")) {
        std::string op = cur.text;
        consume();
        long long rhs = 0;
        if (!parseMul(rhs)) return false;
        if (op == "+") outVal = outVal + rhs;
        else outVal = outVal - rhs;
      }
      return true;
    }

    bool parseShift(long long& outVal) {
      if (!parseAdd(outVal)) return false;
      while (cur.kind == Token::Op && (cur.text == "<<" || cur.text == ">>")) {
        std::string op = cur.text;
        consume();
        long long rhs = 0;
        if (!parseAdd(rhs)) return false;
        if (op == "<<") outVal = outVal << rhs;
        else outVal = outVal >> rhs;
      }
      return true;
    }

    bool parseRel(long long& outVal) {
      if (!parseShift(outVal)) return false;
      while (cur.kind == Token::Op &&
             (cur.text == "<" || cur.text == "<=" || cur.text == ">" || cur.text == ">=")) {
        std::string op = cur.text;
        consume();
        long long rhs = 0;
        if (!parseShift(rhs)) return false;
        if (op == "<") outVal = (outVal < rhs) ? 1 : 0;
        else if (op == "<=") outVal = (outVal <= rhs) ? 1 : 0;
        else if (op == ">") outVal = (outVal > rhs) ? 1 : 0;
        else outVal = (outVal >= rhs) ? 1 : 0;
      }
      return true;
    }

    bool parseEq(long long& outVal) {
      if (!parseRel(outVal)) return false;
      while (cur.kind == Token::Op && (cur.text == "==" || cur.text == "!=")) {
        std::string op = cur.text;
        consume();
        long long rhs = 0;
        if (!parseRel(rhs)) return false;
        if (op == "==") outVal = (outVal == rhs) ? 1 : 0;
        else outVal = (outVal != rhs) ? 1 : 0;
      }
      return true;
    }

    bool parseBitAnd(long long& outVal) {
      if (!parseEq(outVal)) return false;
      while (cur.kind == Token::Op && cur.text == "&") {
        consume();
        long long rhs = 0;
        if (!parseEq(rhs)) return false;
        outVal = outVal & rhs;
      }
      return true;
    }

    bool parseBitXor(long long& outVal) {
      if (!parseBitAnd(outVal)) return false;
      while (cur.kind == Token::Op && cur.text == "^") {
        consume();
        long long rhs = 0;
        if (!parseBitAnd(rhs)) return false;
        outVal = outVal ^ rhs;
      }
      return true;
    }

    bool parseBitOr(long long& outVal) {
      if (!parseBitXor(outVal)) return false;
      while (cur.kind == Token::Op && cur.text == "|") {
        consume();
        long long rhs = 0;
        if (!parseBitXor(rhs)) return false;
        outVal = outVal | rhs;
      }
      return true;
    }

    bool parseLogicalAnd(long long& outVal) {
      if (!parseBitOr(outVal)) return false;
      while (cur.kind == Token::Op && cur.text == "&&") {
        consume();
        long long rhs = 0;
        if (!parseBitOr(rhs)) return false;
        outVal = (outVal && rhs) ? 1 : 0;
      }
      return true;
    }

    bool parseExpr(long long& outVal) {
      if (!parseLogicalAnd(outVal)) return false;
      while (cur.kind == Token::Op && cur.text == "||") {
        consume();
        long long rhs = 0;
        if (!parseLogicalAnd(rhs)) return false;
        outVal = (outVal || rhs) ? 1 : 0;
      }
      return true;
    }
  };

  std::string t = trim(expr);
  if (t.empty()) {
    err = "expected expression after '#if'";
    return false;
  }

  Parser p(*this, t);
  long long value = 0;
  if (!p.parseExpr(value)) {
    err = p.err.empty() ? "invalid #if expression" : p.err;
    return false;
  }
  if (p.cur.kind != Token::End) {
    err = "invalid #if expression";
    return false;
  }
  out = value != 0;
  return true;
}

std::string Preprocessor::expandLine(const std::string& line, const std::string& path, int lineNo) {
  size_t commentPos = line.find("//");
  std::string code = line;
  std::string comment;
  if (commentPos != std::string::npos) {
    code = line.substr(0, commentPos);
    comment = line.substr(commentPos);
  }

  std::unordered_map<std::string, bool> expanding;
  std::string expanded = expandText(code, path, lineNo, expanding, 0);
  if (!comment.empty()) expanded += comment;
  return expanded;
}

std::string Preprocessor::expandText(
    const std::string& text, const std::string& path, int lineNo,
    std::unordered_map<std::string, bool>& expanding, int depth) {
  if (depth > 32) return text;
  std::string out;
  size_t i = 0;
  while (i < text.size()) {
    char c = text[i];
    if (c == '"' || c == '\'') {
      char quote = c;
      out.push_back(c);
      i++;
      while (i < text.size()) {
        char cc = text[i];
        out.push_back(cc);
        i++;
        if (cc == '\\' && i < text.size()) {
          out.push_back(text[i]);
          i++;
          continue;
        }
        if (cc == quote) break;
      }
      continue;
    }
    if (isIdentStart(c)) {
      size_t start = i;
      i++;
      while (i < text.size() && isIdentChar(text[i])) i++;
      std::string name = text.substr(start, i - start);
      if (name == "__LINE__") {
        out += std::to_string(lineNo);
        continue;
      }
      if (name == "__FILE__") {
        out += toStringLiteral(path);
        continue;
      }
      if (name == "__DATE__") {
        out += toStringLiteral(builtinDate_);
        continue;
      }
      if (name == "__TIME__") {
        out += toStringLiteral(builtinTime_);
        continue;
      }
      auto it = macros_.find(name);
      if (it != macros_.end() && !expanding[name]) {
        const Macro& macro = it->second;
        if (macro.functionLike) {
          if (i < text.size() && text[i] == '(') {
            size_t pos = i + 1;
            int parenDepth = 1;
            std::vector<std::string> args;
            std::string current;
            bool sawSep = false;
            bool ok = false;
            while (pos < text.size()) {
              char cc = text[pos];
              if (cc == '"' || cc == '\'') {
                char quote = cc;
                current.push_back(cc);
                pos++;
                while (pos < text.size()) {
                  char qc = text[pos];
                  current.push_back(qc);
                  pos++;
                  if (qc == '\\' && pos < text.size()) {
                    current.push_back(text[pos]);
                    pos++;
                    continue;
                  }
                  if (qc == quote) break;
                }
                continue;
              }
              if (cc == '(') {
                parenDepth++;
                current.push_back(cc);
                pos++;
                continue;
              }
              if (cc == ')') {
                parenDepth--;
                if (parenDepth == 0) {
                  std::string trimmed = trim(current);
                  if (sawSep || !trimmed.empty()) args.push_back(trimmed);
                  pos++;
                  ok = true;
                  break;
                }
                current.push_back(cc);
                pos++;
                continue;
              }
              if (cc == ',' && parenDepth == 1) {
                args.push_back(trim(current));
                current.clear();
                sawSep = true;
                pos++;
                continue;
              }
              current.push_back(cc);
              pos++;
            }
            if (ok) {
              size_t fixedCount = macro.params.size();
              if ((!macro.variadic && args.size() == fixedCount) ||
                  (macro.variadic && args.size() >= fixedCount)) {
                std::vector<std::string> expArgs;
                expArgs.reserve(args.size());
                for (const auto& a : args) {
                  expArgs.push_back(expandText(a, path, lineNo, expanding, depth + 1));
                }
                std::string varRaw;
                std::string varExpanded;
                if (macro.variadic) {
                  for (size_t ai = fixedCount; ai < args.size(); ++ai) {
                    if (ai > fixedCount) {
                      varRaw += ",";
                      varExpanded += ",";
                    }
                    varRaw += args[ai];
                    varExpanded += expArgs[ai];
                  }
                }
                std::vector<std::string> fixedRaw;
                std::vector<std::string> fixedExpanded;
                fixedRaw.reserve(fixedCount);
                fixedExpanded.reserve(fixedCount);
                for (size_t ai = 0; ai < fixedCount; ++ai) {
                  fixedRaw.push_back(args[ai]);
                  fixedExpanded.push_back(expArgs[ai]);
                }
                std::string replaced = replaceParams(
                    macro.body, macro.params, fixedRaw, fixedExpanded,
                    macro.variadic, varRaw, varExpanded);
                expanding[name] = true;
                out += expandText(replaced, path, lineNo, expanding, depth + 1);
                expanding[name] = false;
                i = pos;
                continue;
              }
            }
          }
          out += name;
        } else {
          expanding[name] = true;
          out += expandText(macro.body, path, lineNo, expanding, depth + 1);
          expanding[name] = false;
        }
      } else {
        out += name;
      }
      continue;
    }
    out.push_back(c);
    i++;
  }
  return out;
}

bool Preprocessor::report(const std::string& path, int line, int col, const std::string& msg) {
  std::ostringstream oss;
  oss << path << ":" << line << ":" << col << ": error: " << msg;
  errors_.push_back(oss.str());
  for (const auto& e : errors_) {
    std::cerr << e << "\n";
  }
  return false;
}

bool Preprocessor::readFile(const std::string& path, std::string& out) {
  std::ifstream ifs(path);
  if (!ifs) return false;
  std::stringstream ss;
  ss << ifs.rdbuf();
  out = ss.str();
  return true;
}

std::string Preprocessor::dirName(const std::string& path) {
  size_t pos = path.find_last_of("/\\");
  if (pos == std::string::npos) return "";
  return path.substr(0, pos);
}

} // namespace c99cc
