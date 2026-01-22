#include "parser.h"

#include <functional>

namespace c99cc {

void Parser::advance() {
  if (hasPeek_) {
    cur_ = peek_;
    hasPeek_ = false;
  } else {
    cur_ = lex_.next();
  }
}

const Token& Parser::peekToken() {
  if (!hasPeek_) {
    peek_ = lex_.next();
    hasPeek_ = true;
  }
  return peek_;
}

bool Parser::expect(TokenKind k, const char* what) {
  if (cur_.kind == k) return true;
  diags_.error(cur_.loc, std::string("expected ") + what);
  return false;
}

Parser::PtrQuals Parser::parsePointerQuals() {
  PtrQuals out;
  while (cur_.kind == TokenKind::Star) {
    out.depth++;
    advance();
    bool isConst = false;
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
    }
    out.consts.push_back(isConst);
  }
  return out;
}

std::optional<Parser::ParsedTypeSpec> Parser::parseTypeSpec(bool allowStructDef, bool allowStorage) {
  ParsedTypeSpec spec;
  SourceLocation typeLoc = cur_.loc;
  bool isConst = false;
  bool isStatic = false;
  bool saw = true;
  while (saw) {
    saw = false;
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
      saw = true;
    }
    if (allowStorage && cur_.kind == TokenKind::KwStatic) {
      isStatic = true;
      advance();
      saw = true;
    }
  }
  if (cur_.kind == TokenKind::KwUnsigned) {
    advance();
    spec.type.isUnsigned = true;
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
    }
    if (cur_.kind == TokenKind::KwFloat || cur_.kind == TokenKind::KwDouble ||
        cur_.kind == TokenKind::KwVoid || cur_.kind == TokenKind::KwStruct) {
      diags_.error(cur_.loc, "expected integer type after 'unsigned'");
      return std::nullopt;
    }
    if (cur_.kind == TokenKind::KwChar) {
      advance();
      spec.type.base = Type::Base::Char;
      spec.type.isConst = isConst;
      spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
      return spec;
    }
    if (cur_.kind == TokenKind::KwShort) {
      advance();
      spec.type.base = Type::Base::Short;
      spec.type.isConst = isConst;
      spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
      return spec;
    }
    if (cur_.kind == TokenKind::KwInt) {
      advance();
      spec.type.base = Type::Base::Int;
      spec.type.isConst = isConst;
      spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
      return spec;
    }
    if (cur_.kind == TokenKind::KwLong) {
      advance();
      if (cur_.kind == TokenKind::KwLong) {
        advance();
        spec.type.base = Type::Base::LongLong;
        spec.type.isConst = isConst;
        spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
        return spec;
      }
      spec.type.base = Type::Base::Long;
      spec.type.isConst = isConst;
      spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
      return spec;
    }
    spec.type.base = Type::Base::Int;
    spec.type.isConst = isConst;
    spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
    return spec;
  }
  if (cur_.kind == TokenKind::KwChar) {
    advance();
    spec.type.base = Type::Base::Char;
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
    }
    spec.type.isConst = isConst;
    spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
    return spec;
  }
  if (cur_.kind == TokenKind::KwShort) {
    advance();
    spec.type.base = Type::Base::Short;
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
    }
    spec.type.isConst = isConst;
    spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
    return spec;
  }
  if (cur_.kind == TokenKind::KwInt) {
    advance();
    spec.type.base = Type::Base::Int;
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
    }
    spec.type.isConst = isConst;
    spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
    return spec;
  }
  if (cur_.kind == TokenKind::KwLong) {
    advance();
    if (cur_.kind == TokenKind::KwLong) {
      advance();
      spec.type.base = Type::Base::LongLong;
      while (cur_.kind == TokenKind::KwConst) {
        isConst = true;
        advance();
      }
      spec.type.isConst = isConst;
      spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
      return spec;
    }
    spec.type.base = Type::Base::Long;
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
    }
    spec.type.isConst = isConst;
    spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
    return spec;
  }
  if (cur_.kind == TokenKind::KwFloat) {
    advance();
    spec.type.base = Type::Base::Float;
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
    }
    spec.type.isConst = isConst;
    spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
    return spec;
  }
  if (cur_.kind == TokenKind::KwDouble) {
    advance();
    spec.type.base = Type::Base::Double;
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
    }
    spec.type.isConst = isConst;
    spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
    return spec;
  }
  if (cur_.kind == TokenKind::KwVoid) {
    advance();
    spec.type.base = Type::Base::Void;
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
    }
    spec.type.isConst = isConst;
    spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
    return spec;
  }
  if (cur_.kind == TokenKind::KwEnum) {
    advance();
    spec.type.base = Type::Base::Enum;
    if (cur_.kind == TokenKind::Identifier) {
      spec.type.enumName = cur_.text;
      advance();
    }
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
    }
    spec.type.isConst = isConst;
    spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
    if (cur_.kind == TokenKind::LBrace) {
      if (!allowStructDef) {
        diags_.error(cur_.loc, "enum definition not allowed here");
        return std::nullopt;
      }
      advance();
      auto items = parseEnumItems();
      if (!items) return std::nullopt;
      if (!expect(TokenKind::RBrace, "'}'")) return std::nullopt;
      advance();
      EnumDef def;
      def.name = spec.type.enumName.empty()
                     ? std::optional<std::string>{}
                     : std::optional<std::string>{spec.type.enumName};
      def.nameLoc = typeLoc;
      def.items = std::move(*items);
      spec.enumDef = std::move(def);
    } else if (spec.type.enumName.empty()) {
      diags_.error(cur_.loc, "expected enum name or definition");
      return std::nullopt;
    }
    return spec;
  }
  if (cur_.kind == TokenKind::KwStruct) {
    advance();
    if (!expect(TokenKind::Identifier, "struct name")) return std::nullopt;
    std::string name = cur_.text;
    SourceLocation nameLoc = cur_.loc;
    advance();
    spec.type.base = Type::Base::Struct;
    spec.type.structName = name;
    while (cur_.kind == TokenKind::KwConst) {
      isConst = true;
      advance();
    }
    spec.type.isConst = isConst;
    spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
    if (cur_.kind == TokenKind::LBrace) {
      if (!allowStructDef) {
        diags_.error(cur_.loc, "struct definition not allowed here");
        return std::nullopt;
      }
      advance();
      auto fields = parseStructFields();
      if (!fields) return std::nullopt;
      if (!expect(TokenKind::RBrace, "'}'")) return std::nullopt;
      advance();
      StructDef def;
      def.name = std::move(name);
      def.nameLoc = nameLoc;
      def.fields = std::move(*fields);
      spec.structDef = std::move(def);
    }
    return spec;
  }
  if (cur_.kind == TokenKind::Identifier) {
    auto it = typedefs_.find(cur_.text);
    if (it != typedefs_.end()) {
      spec.type = it->second;
      advance();
      while (cur_.kind == TokenKind::KwConst) {
        spec.type.isConst = true;
        advance();
      }
      spec.storage = isStatic ? StorageClass::Static : StorageClass::None;
      return spec;
    }
  }
  return std::nullopt;
}

std::optional<std::vector<StructField>> Parser::parseStructFields() {
  std::vector<StructField> fields;
  while (cur_.kind != TokenKind::RBrace && cur_.kind != TokenKind::Eof) {
    auto baseSpec = parseTypeSpec(/*allowStructDef=*/false, /*allowStorage=*/false);
    if (!baseSpec) return std::nullopt;
    Type baseType = baseSpec->type;

    while (true) {
      auto decl = parseDeclarator(baseType, /*allowArray=*/true, /*allowFirstEmpty=*/false,
                                  /*allowFunctionPointer=*/true);
      if (!decl) return std::nullopt;
      StructField f;
      f.type = decl->type;
      f.name = std::move(decl->name);
      f.nameLoc = decl->nameLoc;
      fields.push_back(std::move(f));

      if (cur_.kind != TokenKind::Comma) break;
      advance();
    }

    if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
    advance();
  }
  return fields;
}

std::optional<std::vector<EnumItem>> Parser::parseEnumItems() {
  std::vector<EnumItem> items;
  int64_t current = -1;
  while (cur_.kind != TokenKind::RBrace && cur_.kind != TokenKind::Eof) {
    if (!expect(TokenKind::Identifier, "identifier")) return std::nullopt;
    EnumItem item;
    item.name = cur_.text;
    item.nameLoc = cur_.loc;
    advance();
    int64_t value = current + 1;
    if (cur_.kind == TokenKind::Assign) {
      advance();
      bool neg = false;
      if (cur_.kind == TokenKind::Minus || cur_.kind == TokenKind::Plus) {
        neg = (cur_.kind == TokenKind::Minus);
        advance();
      }
      if (cur_.kind == TokenKind::IntegerLiteral) {
        value = std::stoll(cur_.text);
        if (neg) value = -value;
        advance();
      } else if (cur_.kind == TokenKind::Identifier) {
        auto it = enumConstants_.find(cur_.text);
        if (it == enumConstants_.end()) {
          diags_.error(cur_.loc, "unknown enum constant '" + cur_.text + "'");
          return std::nullopt;
        }
        value = it->second;
        if (neg) value = -value;
        advance();
      } else {
        diags_.error(cur_.loc, "expected integer literal or enum constant");
        return std::nullopt;
      }
    }
    if (enumConstants_.count(item.name)) {
      diags_.error(item.nameLoc, "redefinition of enum constant '" + item.name + "'");
      return std::nullopt;
    }
    item.value = value;
    enumConstants_.emplace(item.name, value);
    items.push_back(std::move(item));
    current = value;

    if (cur_.kind == TokenKind::Comma) {
      advance();
      continue;
    }
    break;
  }
  return items;
}

std::optional<std::vector<std::optional<size_t>>> Parser::parseArrayDims(bool allowFirstEmpty) {
  std::vector<std::optional<size_t>> dims;
  while (cur_.kind == TokenKind::LBracket) {
    advance();
    if (cur_.kind == TokenKind::RBracket) {
      if (allowFirstEmpty && dims.empty()) {
        dims.push_back(std::nullopt);
        advance();
        continue;
      }
      diags_.error(cur_.loc, "expected integer literal in array size");
      return std::nullopt;
    }
    if (cur_.kind != TokenKind::IntegerLiteral) {
      diags_.error(cur_.loc, "expected integer literal in array size");
      return std::nullopt;
    }
    size_t size = static_cast<size_t>(std::stoll(cur_.text));
    dims.push_back(size);
    advance();
    if (!expect(TokenKind::RBracket, "']'")) return std::nullopt;
    advance();
  }
  if (dims.empty()) return std::nullopt;
  return dims;
}

std::optional<Type> Parser::parseTypeName(bool allowStructDef) {
  auto specOpt = parseTypeSpec(allowStructDef, /*allowStorage=*/false);
  if (!specOpt) return std::nullopt;
  if (specOpt->structDef || specOpt->enumDef) {
    diags_.error(cur_.loc, "type definition not allowed here");
    return std::nullopt;
  }
  Type t = specOpt->type;
  auto quals = parsePointerQuals();
  t.addPointerQuals(quals.consts);
  if (cur_.kind == TokenKind::LBracket) {
    auto dims = parseArrayDims(/*allowFirstEmpty=*/false);
    if (!dims) return std::nullopt;
    t.arrayDims = std::move(*dims);
  }
  return t;
}

std::optional<Declarator> Parser::parseDeclarator(const Type& baseType, bool allowArray,
                                                  bool allowFirstEmpty,
                                                  bool allowFunctionPointer) {
  Declarator d;
  auto retQuals = parsePointerQuals();
  if (allowFunctionPointer && cur_.kind == TokenKind::LParen && peekToken().kind == TokenKind::Star) {
    advance(); // '('
    advance(); // '*'
    bool ptrIsConst = false;
    while (cur_.kind == TokenKind::KwConst) {
      ptrIsConst = true;
      advance();
    }
    if (!expect(TokenKind::Identifier, "identifier")) return std::nullopt;
    d.name = cur_.text;
    d.nameLoc = cur_.loc;
    advance();
    std::optional<std::vector<std::optional<size_t>>> dims;
    if (allowArray && cur_.kind == TokenKind::LBracket) {
      dims = parseArrayDims(/*allowFirstEmpty=*/allowFirstEmpty);
      if (!dims) return std::nullopt;
    }
    if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
    advance();
    if (!expect(TokenKind::LParen, "'('")) return std::nullopt;
    advance();
    auto fnParams = parseParamList();
    if (!fnParams) return std::nullopt;
    if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
    advance();

    auto fnTy = std::make_shared<FunctionType>();
    fnTy->returnType = baseType;
    fnTy->returnType.addPointerQuals(retQuals.consts);
    fnTy->isVariadic = fnParams->isVariadic;
    for (const auto& param : fnParams->params) {
      fnTy->params.push_back(param.type);
    }

    d.type = baseType;
    d.type.ptrDepth = 0;
    d.type.ptrConst.clear();
    d.type.addPointerLevel(ptrIsConst);
    d.type.func = std::move(fnTy);
    if (dims) d.type.arrayDims = std::move(*dims);
    return d;
  }

  if (!expect(TokenKind::Identifier, "identifier")) return std::nullopt;
  d.name = cur_.text;
  d.nameLoc = cur_.loc;
  advance();
  d.type = baseType;
  d.type.addPointerQuals(retQuals.consts);
  if (allowArray && cur_.kind == TokenKind::LBracket) {
    auto dims = parseArrayDims(/*allowFirstEmpty=*/allowFirstEmpty);
    if (!dims) return std::nullopt;
    d.type.arrayDims = std::move(*dims);
  }
  return d;
}

// -------------------- TU / top-level --------------------

std::optional<Parser::ParamList> Parser::parseParamList() {
  // params := ε | type_spec '*'* [ident]? (',' type_spec '*'* [ident]?)*
  ParamList list;

  if (cur_.kind == TokenKind::RParen) return list; // empty

  while (true) {
    if (cur_.kind == TokenKind::Ellipsis) {
      list.isVariadic = true;
      advance();
      break;
    }
    SourceLocation typeLoc = cur_.loc;
    Param p;
    Type baseType;
    PtrQuals retQuals;
    if (cur_.kind == TokenKind::KwVoid) {
      advance();
      if (cur_.kind == TokenKind::RParen) {
        // "void)" means no parameters
        return list;
      }
      baseType.base = Type::Base::Void;
      retQuals = parsePointerQuals();
    } else {
      auto specOpt = parseTypeSpec(/*allowStructDef=*/false, /*allowStorage=*/false);
      if (!specOpt) {
        diags_.error(cur_.loc, "expected type");
        return std::nullopt;
      }
      baseType = specOpt->type;
      retQuals = parsePointerQuals();
    }
    p.loc = typeLoc;

    if (cur_.kind == TokenKind::LParen && peekToken().kind == TokenKind::Star) {
      // function pointer parameter: ret_type (*name?)(params)
      advance(); // '('
      advance(); // '*'
      bool ptrIsConst = false;
      while (cur_.kind == TokenKind::KwConst) {
        ptrIsConst = true;
        advance();
      }
      if (cur_.kind == TokenKind::Identifier) {
        p.name = cur_.text;
        p.nameLoc = cur_.loc;
        advance();
      } else {
        p.name = std::nullopt;
        p.nameLoc = SourceLocation{};
      }
      if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
      advance();
      if (!expect(TokenKind::LParen, "'('")) return std::nullopt;
      advance();
      auto fnParams = parseParamList();
      if (!fnParams) return std::nullopt;
      if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
      advance();

      auto fnTy = std::make_shared<FunctionType>();
      fnTy->returnType = baseType;
      fnTy->returnType.addPointerQuals(retQuals.consts);
      fnTy->isVariadic = fnParams->isVariadic;
      for (const auto& param : fnParams->params) {
        fnTy->params.push_back(param.type);
      }

      p.type = baseType;
      p.type.ptrDepth = 0;
      p.type.ptrConst.clear();
      p.type.addPointerLevel(ptrIsConst);
      p.type.func = std::move(fnTy);
    } else {
      p.type = baseType;
      p.type.addPointerQuals(retQuals.consts);
      if (cur_.kind == TokenKind::Identifier) {
        p.name = cur_.text;
        p.nameLoc = cur_.loc;
        advance();
        if (cur_.kind == TokenKind::LBracket) {
          auto dims = parseArrayDims(/*allowFirstEmpty=*/true);
          if (!dims) return std::nullopt;
          p.type.arrayDims = std::move(*dims);
        }
      } else {
        p.name = std::nullopt;
        p.nameLoc = SourceLocation{}; // unused
      }
    }

    list.params.push_back(std::move(p));

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

  return list;
}

std::optional<FunctionProto> Parser::parseFunctionProto() {
  // type_spec <name> '(' params ')'
  auto specOpt = parseTypeSpec(/*allowStructDef=*/false, /*allowStorage=*/false);
  if (!specOpt) {
    diags_.error(cur_.loc, "expected return type");
    return std::nullopt;
  }
  auto retQuals = parsePointerQuals();

  if (!expect(TokenKind::Identifier, "identifier")) return std::nullopt;
  FunctionProto proto;
  proto.returnType = specOpt->type;
  proto.returnType.addPointerQuals(retQuals.consts);
  proto.name = cur_.text;
  proto.nameLoc = cur_.loc;
  advance();

  if (!expect(TokenKind::LParen, "'('")) return std::nullopt;
  advance();

  auto params = parseParamList();
  if (!params) return std::nullopt;
  proto.params = std::move(params->params);
  proto.isVariadic = params->isVariadic;

  if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
  advance();

  return proto;
}

std::optional<FunctionDef> Parser::parseFunctionDefAfterProto(FunctionProto proto) {
  // expects current token is '{'
  if (!expect(TokenKind::LBrace, "'{'")) return std::nullopt;
  advance();

  FunctionDef def;
  def.proto = std::move(proto);

  while (cur_.kind != TokenKind::RBrace && cur_.kind != TokenKind::Eof) {
    auto s = parseStmt();
    if (!s) return std::nullopt;
    def.body.push_back(std::move(*s));
  }

  if (!expect(TokenKind::RBrace, "'}'")) return std::nullopt;
  advance();
  return def;
}

std::optional<TopLevelItem> Parser::parseTopLevelItem() {
  if (!pending_.empty()) {
    TopLevelItem item = std::move(pending_.front());
    pending_.erase(pending_.begin());
    return item;
  }

  if (cur_.kind == TokenKind::KwTypedef) {
    return parseTypedefTopLevel();
  }

  auto specOpt = parseTypeSpec(/*allowStructDef=*/true, /*allowStorage=*/true);
  if (!specOpt) {
    diags_.error(cur_.loc, "expected type");
    return std::nullopt;
  }

  if (specOpt->structDef && cur_.kind == TokenKind::Semicolon) {
    if (specOpt->storage != StorageClass::None) {
      diags_.error(cur_.loc, "storage class not allowed here");
      return std::nullopt;
    }
    StructDef def = std::move(*specOpt->structDef);
    advance();
    return TopLevelItem{std::move(def)};
  }
  if (specOpt->enumDef && cur_.kind == TokenKind::Semicolon) {
    if (specOpt->storage != StorageClass::None) {
      diags_.error(cur_.loc, "storage class not allowed here");
      return std::nullopt;
    }
    EnumDef def = std::move(*specOpt->enumDef);
    advance();
    return TopLevelItem{std::move(def)};
  }

  Type baseType = specOpt->type;
  auto firstDecl = parseDeclarator(baseType, /*allowArray=*/true, /*allowFirstEmpty=*/true,
                                   /*allowFunctionPointer=*/true);
  if (!firstDecl) return std::nullopt;

  if (!firstDecl->type.func && cur_.kind == TokenKind::LParen) {
    advance(); // '('

    auto params = parseParamList();
    if (!params) return std::nullopt;

    FunctionProto proto;
    proto.returnType = firstDecl->type;
    proto.name = std::move(firstDecl->name);
    proto.nameLoc = firstDecl->nameLoc;
    proto.params = std::move(params->params);
    proto.isVariadic = params->isVariadic;
    proto.storage = specOpt->storage;

    if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
    advance();

    if (cur_.kind == TokenKind::Semicolon) {
      FunctionDecl decl;
      decl.proto = std::move(proto);
      decl.semiLoc = cur_.loc;
      advance();
      if (specOpt->structDef) {
        pending_.push_back(TopLevelItem{std::move(decl)});
        return TopLevelItem{std::move(*specOpt->structDef)};
      }
      if (specOpt->enumDef) {
        pending_.push_back(TopLevelItem{std::move(decl)});
        return TopLevelItem{std::move(*specOpt->enumDef)};
      }
      return TopLevelItem{std::move(decl)};
    }

    if (cur_.kind == TokenKind::LBrace) {
      auto def = parseFunctionDefAfterProto(std::move(proto));
      if (!def) return std::nullopt;
      if (specOpt->structDef) {
        pending_.push_back(TopLevelItem{std::move(*def)});
        return TopLevelItem{std::move(*specOpt->structDef)};
      }
      if (specOpt->enumDef) {
        pending_.push_back(TopLevelItem{std::move(*def)});
        return TopLevelItem{std::move(*specOpt->enumDef)};
      }
      return TopLevelItem{std::move(*def)};
    }

    diags_.error(cur_.loc, "expected ';' or '{' after function prototype");
    return std::nullopt;
  }

  std::vector<DeclItem> items;
  DeclItem first;
  first.type = firstDecl->type;
  first.name = std::move(firstDecl->name);
  first.nameLoc = firstDecl->nameLoc;
  first.storage = specOpt->storage;

  if (cur_.kind == TokenKind::Assign) {
    advance();
    auto e = parseInitializer();
    if (!e) return std::nullopt;
    first.initExpr = std::move(*e);
  }

  items.push_back(std::move(first));

  while (cur_.kind == TokenKind::Comma) {
    advance();
    auto decl = parseDeclarator(baseType, /*allowArray=*/true, /*allowFirstEmpty=*/true,
                                /*allowFunctionPointer=*/true);
    if (!decl) return std::nullopt;
    DeclItem item;
    item.type = decl->type;
    item.name = std::move(decl->name);
    item.nameLoc = decl->nameLoc;
    item.storage = specOpt->storage;

    if (cur_.kind == TokenKind::Assign) {
      advance();
      auto e = parseInitializer();
      if (!e) return std::nullopt;
      item.initExpr = std::move(*e);
    }

    items.push_back(std::move(item));
  }

  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();

  GlobalVarDecl decl;
  decl.items = std::move(items);
  if (specOpt->structDef) {
    pending_.push_back(TopLevelItem{std::move(decl)});
    return TopLevelItem{std::move(*specOpt->structDef)};
  }
  if (specOpt->enumDef) {
    pending_.push_back(TopLevelItem{std::move(decl)});
    return TopLevelItem{std::move(*specOpt->enumDef)};
  }
  return TopLevelItem{std::move(decl)};
}

std::optional<AstTranslationUnit> Parser::parseTranslationUnit() {
  AstTranslationUnit tu;

  while (cur_.kind != TokenKind::Eof) {
    auto item = parseTopLevelItem();
    if (!item) return std::nullopt;
    tu.items.push_back(std::move(*item));
  }

  return tu;
}

// -------------------- statements --------------------

std::optional<std::unique_ptr<Stmt>> Parser::parseStmt() {
  if (cur_.kind == TokenKind::KwTypedef) return parseTypedefStmt();
  if (cur_.kind == TokenKind::KwStatic || cur_.kind == TokenKind::KwConst ||
      cur_.kind == TokenKind::KwChar ||
      cur_.kind == TokenKind::KwShort ||
      cur_.kind == TokenKind::KwInt || cur_.kind == TokenKind::KwLong ||
      cur_.kind == TokenKind::KwUnsigned || cur_.kind == TokenKind::KwFloat ||
      cur_.kind == TokenKind::KwDouble ||
      cur_.kind == TokenKind::KwVoid || cur_.kind == TokenKind::KwStruct ||
      cur_.kind == TokenKind::KwEnum ||
      (cur_.kind == TokenKind::Identifier && typedefs_.count(cur_.text) > 0)) {
    return parseDeclStmt();
  }
  if (cur_.kind == TokenKind::KwReturn) return parseReturnStmt();
  if (cur_.kind == TokenKind::KwIf) return parseIfStmt();
  if (cur_.kind == TokenKind::KwWhile) return parseWhileStmt();
  if (cur_.kind == TokenKind::KwDo) return parseDoWhileStmt();
  if (cur_.kind == TokenKind::KwFor) return parseForStmt();
  if (cur_.kind == TokenKind::KwSwitch) return parseSwitchStmt();
  if (cur_.kind == TokenKind::KwBreak) return parseBreakStmt();
  if (cur_.kind == TokenKind::KwContinue) return parseContinueStmt();
  if (cur_.kind == TokenKind::LBrace) return parseBlockStmt();

  if (cur_.kind == TokenKind::Semicolon) {
    SourceLocation l = cur_.loc;
    advance();
    return std::make_unique<EmptyStmt>(l);
  }

  // expression statement
  SourceLocation l = cur_.loc;
  auto e = parseExpr();
  if (!e) return std::nullopt;
  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();
  return std::make_unique<ExprStmt>(l, std::move(*e));
}

std::optional<std::unique_ptr<Stmt>> Parser::parseDeclStmt() {
  SourceLocation l = cur_.loc;
  auto specOpt = parseTypeSpec(/*allowStructDef=*/false, /*allowStorage=*/true);
  if (!specOpt) {
    diags_.error(cur_.loc, "expected type");
    return std::nullopt;
  }

  std::vector<DeclItem> items;
  Type baseType = specOpt->type;

  while (true) {
    auto decl = parseDeclarator(baseType, /*allowArray=*/true, /*allowFirstEmpty=*/true,
                                /*allowFunctionPointer=*/true);
    if (!decl) return std::nullopt;
    DeclItem item;
    item.type = decl->type;
    item.name = std::move(decl->name);
    item.nameLoc = decl->nameLoc;
    item.storage = specOpt->storage;

    if (cur_.kind == TokenKind::Assign) {
      advance();
      auto e = parseInitializer();
      if (!e) return std::nullopt;
      item.initExpr = std::move(*e);
    }

    items.push_back(std::move(item));

    if (cur_.kind != TokenKind::Comma) break;
    advance();
  }

  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();

  return std::make_unique<DeclStmt>(l, std::move(items));
}

std::optional<std::vector<DeclItem>> Parser::parseTypedefItems(bool allowStructDef) {
  if (!expect(TokenKind::KwTypedef, "'typedef'")) return std::nullopt;
  advance();
  auto specOpt = parseTypeSpec(allowStructDef, /*allowStorage=*/false);
  if (!specOpt) {
    diags_.error(cur_.loc, "expected type");
    return std::nullopt;
  }
  if (specOpt->storage != StorageClass::None) {
    diags_.error(cur_.loc, "storage class not allowed in typedef");
    return std::nullopt;
  }
  if (specOpt->structDef) {
    pending_.push_back(TopLevelItem{std::move(*specOpt->structDef)});
  }
  if (specOpt->enumDef) {
    pending_.push_back(TopLevelItem{std::move(*specOpt->enumDef)});
  }

  std::vector<DeclItem> items;
  Type baseType = specOpt->type;

  while (true) {
    auto decl = parseDeclarator(baseType, /*allowArray=*/true, /*allowFirstEmpty=*/false,
                                /*allowFunctionPointer=*/true);
    if (!decl) return std::nullopt;
    DeclItem item;
    item.type = decl->type;
    item.name = std::move(decl->name);
    item.nameLoc = decl->nameLoc;
    if (typedefs_.count(item.name)) {
      diags_.error(item.nameLoc, "redefinition of typedef '" + item.name + "'");
      return std::nullopt;
    }
    typedefs_.emplace(item.name, item.type);
    items.push_back(std::move(item));
    if (cur_.kind != TokenKind::Comma) break;
    advance();
  }
  if (!expect(TokenKind::Semicolon, "';'")) return std::nullopt;
  advance();
  return items;
}

std::optional<TopLevelItem> Parser::parseTypedefTopLevel() {
  auto items = parseTypedefItems(/*allowStructDef=*/true);
  if (!items) return std::nullopt;
  TypedefDecl decl;
  decl.items = std::move(*items);
  return TopLevelItem{std::move(decl)};
}

std::optional<std::unique_ptr<Stmt>> Parser::parseTypedefStmt() {
  SourceLocation l = cur_.loc;
  auto items = parseTypedefItems(/*allowStructDef=*/false);
  if (!items) return std::nullopt;
  return std::make_unique<TypedefStmt>(l, std::move(*items));
}

std::optional<std::unique_ptr<Stmt>> Parser::parseAssignStmt() {
  // legacy path; not used by parseStmt() anymore
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

  if (cur_.kind == TokenKind::Semicolon) {
    advance();
    return std::make_unique<ReturnStmt>(l);
  }

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

  std::unique_ptr<Stmt> init = nullptr;

  if (cur_.kind == TokenKind::Semicolon) {
    advance();
  } else if (cur_.kind == TokenKind::KwChar || cur_.kind == TokenKind::KwShort ||
             cur_.kind == TokenKind::KwInt || cur_.kind == TokenKind::KwLong ||
             cur_.kind == TokenKind::KwUnsigned || cur_.kind == TokenKind::KwFloat ||
             cur_.kind == TokenKind::KwDouble ||
             cur_.kind == TokenKind::KwVoid || cur_.kind == TokenKind::KwStruct) {
    auto d = parseDeclStmt();
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

std::optional<std::unique_ptr<Stmt>> Parser::parseSwitchStmt() {
  SourceLocation sLoc = cur_.loc;
  if (!expect(TokenKind::KwSwitch, "'switch'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::LParen, "'('")) return std::nullopt;
  advance();

  auto cond = parseExpr();
  if (!cond) return std::nullopt;

  if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
  advance();

  if (!expect(TokenKind::LBrace, "'{'")) return std::nullopt;
  advance();

  std::vector<SwitchCase> cases;

  while (cur_.kind != TokenKind::RBrace && cur_.kind != TokenKind::Eof) {
    if (cur_.kind == TokenKind::KwCase) {
      SourceLocation caseLoc = cur_.loc;
      advance();
      if (cur_.kind != TokenKind::IntegerLiteral) {
        diags_.error(cur_.loc, "expected integer literal after 'case'");
        return std::nullopt;
      }
      int64_t value = std::stoll(cur_.text);
      advance();
      if (!expect(TokenKind::Colon, "':'")) return std::nullopt;
      advance();

      SwitchCase c;
      c.value = value;
      c.loc = caseLoc;

      while (cur_.kind != TokenKind::KwCase && cur_.kind != TokenKind::KwDefault &&
             cur_.kind != TokenKind::RBrace && cur_.kind != TokenKind::Eof) {
        auto st = parseStmt();
        if (!st) return std::nullopt;
        c.stmts.push_back(std::move(*st));
      }

      cases.push_back(std::move(c));
      continue;
    }

    if (cur_.kind == TokenKind::KwDefault) {
      SourceLocation defLoc = cur_.loc;
      advance();
      if (!expect(TokenKind::Colon, "':'")) return std::nullopt;
      advance();

      SwitchCase c;
      c.value = std::nullopt;
      c.loc = defLoc;

      while (cur_.kind != TokenKind::KwCase && cur_.kind != TokenKind::KwDefault &&
             cur_.kind != TokenKind::RBrace && cur_.kind != TokenKind::Eof) {
        auto st = parseStmt();
        if (!st) return std::nullopt;
        c.stmts.push_back(std::move(*st));
      }

      cases.push_back(std::move(c));
      continue;
    }

    diags_.error(cur_.loc, "expected 'case' or 'default' in switch");
    return std::nullopt;
  }

  if (!expect(TokenKind::RBrace, "'}'")) return std::nullopt;
  advance();

  return std::make_unique<SwitchStmt>(sLoc, std::move(*cond), std::move(cases));
}

// -------------------- Expression parsing --------------------

std::optional<std::unique_ptr<Expr>> Parser::parsePrimary() {
  if (cur_.kind == TokenKind::IntegerLiteral) {
    SourceLocation l = cur_.loc;
    int64_t v = std::stoll(cur_.text);
    advance();
    return std::make_unique<IntLiteralExpr>(l, v);
  }
  if (cur_.kind == TokenKind::CharLiteral) {
    SourceLocation l = cur_.loc;
    int64_t v = std::stoll(cur_.text);
    advance();
    return std::make_unique<IntLiteralExpr>(l, v);
  }
  if (cur_.kind == TokenKind::FloatLiteral) {
    SourceLocation l = cur_.loc;
    std::string text = cur_.text;
    bool isFloat = false;
    if (!text.empty()) {
      char last = text.back();
      if (last == 'f' || last == 'F') {
        isFloat = true;
        text.pop_back();
      }
    }
    double v = std::stod(text);
    advance();
    return std::make_unique<FloatLiteralExpr>(l, v, isFloat);
  }
  if (cur_.kind == TokenKind::StringLiteral) {
    SourceLocation l = cur_.loc;
    std::string text = cur_.text;
    advance();
    while (cur_.kind == TokenKind::StringLiteral) {
      text += cur_.text;
      advance();
    }
    return std::make_unique<StringLiteralExpr>(l, std::move(text));
  }

  if (cur_.kind == TokenKind::Identifier) {
    SourceLocation idLoc = cur_.loc;
    std::string name = cur_.text;
    advance();
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
  auto isTypeStartToken = [&](const Token& t) {
    if (t.kind == TokenKind::KwChar || t.kind == TokenKind::KwShort ||
        t.kind == TokenKind::KwInt || t.kind == TokenKind::KwLong ||
        t.kind == TokenKind::KwUnsigned || t.kind == TokenKind::KwFloat ||
        t.kind == TokenKind::KwDouble || t.kind == TokenKind::KwVoid ||
        t.kind == TokenKind::KwStruct || t.kind == TokenKind::KwEnum) {
      return true;
    }
    if (t.kind == TokenKind::Identifier) {
      return typedefs_.count(t.text) > 0;
    }
    return false;
  };

  if (cur_.kind == TokenKind::KwSizeof) {
    SourceLocation l = cur_.loc;
    advance();
    if (cur_.kind == TokenKind::LParen && isTypeStartToken(peekToken())) {
      advance();
      auto typeOpt = parseTypeName(/*allowStructDef=*/false);
      if (!typeOpt) return std::nullopt;
      if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
      advance();
      return std::make_unique<SizeofExpr>(l, std::move(*typeOpt));
    }
    auto rhs = parseUnary();
    if (!rhs) return std::nullopt;
    return std::make_unique<SizeofExpr>(l, std::move(*rhs));
  }

  if (cur_.kind == TokenKind::PlusPlus || cur_.kind == TokenKind::MinusMinus) {
    SourceLocation l = cur_.loc;
    bool isInc = cur_.kind == TokenKind::PlusPlus;
    advance();
    auto rhs = parseUnary();
    if (!rhs) return std::nullopt;
    return std::make_unique<IncDecExpr>(l, isInc, /*post=*/false, std::move(*rhs));
  }

  if (cur_.kind == TokenKind::LParen && isTypeStartToken(peekToken())) {
    SourceLocation l = cur_.loc;
    advance();
    auto typeOpt = parseTypeName(/*allowStructDef=*/false);
    if (!typeOpt) return std::nullopt;
    if (!expect(TokenKind::RParen, "')'")) return std::nullopt;
    advance();
    auto rhs = parseUnary();
    if (!rhs) return std::nullopt;
    return std::make_unique<CastExpr>(l, std::move(*typeOpt), std::move(*rhs));
  }

  if (cur_.kind == TokenKind::Plus || cur_.kind == TokenKind::Minus ||
      cur_.kind == TokenKind::Bang || cur_.kind == TokenKind::Tilde ||
      cur_.kind == TokenKind::Star || cur_.kind == TokenKind::Amp) {
    SourceLocation l = cur_.loc;
    TokenKind op = cur_.kind;
    advance();
    auto rhs = parseUnary();
    if (!rhs) return std::nullopt;
    return std::make_unique<UnaryExpr>(l, op, std::move(*rhs));
  }
  return parsePostfix();
}

std::optional<std::unique_ptr<Expr>> Parser::parsePostfix() {
  auto base = parsePrimary();
  if (!base) return std::nullopt;
  while (true) {
    if (cur_.kind == TokenKind::LBracket) {
      SourceLocation l = cur_.loc;
      advance();
      auto idx = parseExpr();
      if (!idx) return std::nullopt;
      if (!expect(TokenKind::RBracket, "']'")) return std::nullopt;
      advance();
      base = std::make_unique<SubscriptExpr>(l, std::move(*base), std::move(*idx));
      continue;
    }
    if (cur_.kind == TokenKind::Dot || cur_.kind == TokenKind::Arrow) {
      SourceLocation l = cur_.loc;
      bool isArrow = cur_.kind == TokenKind::Arrow;
      advance();
      if (!expect(TokenKind::Identifier, "member name")) return std::nullopt;
      std::string member = cur_.text;
      SourceLocation memberLoc = cur_.loc;
      advance();
      base = std::make_unique<MemberExpr>(l, std::move(*base), std::move(member), memberLoc, isArrow);
      continue;
    }
    if (cur_.kind == TokenKind::LParen) {
      SourceLocation l = cur_.loc;
      advance(); // '('
      std::vector<std::unique_ptr<Expr>> args;

      if (cur_.kind != TokenKind::RParen) {
        while (true) {
          auto a = parseAssignmentExpr(); // IMPORTANT: no comma-expr
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

      if (auto* vr = dynamic_cast<VarRefExpr*>(base->get())) {
        SourceLocation calleeLoc = vr->loc;
        std::string name = vr->name;
        base = std::make_unique<CallExpr>(calleeLoc, std::move(name), calleeLoc, std::move(args));
      } else {
        base = std::make_unique<CallExpr>(l, std::move(*base), std::move(args));
      }
      continue;
    }
    if (cur_.kind == TokenKind::PlusPlus || cur_.kind == TokenKind::MinusMinus) {
      SourceLocation l = cur_.loc;
      bool isInc = cur_.kind == TokenKind::PlusPlus;
      advance();
      base = std::make_unique<IncDecExpr>(l, isInc, /*post=*/true, std::move(*base));
      break;
    }
    break;
  }
  return base;
}

// compatibility (unused)
std::optional<std::unique_ptr<Expr>> Parser::parseBinary(int /*minPrec*/) { return parseUnary(); }

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

bool Parser::isBinaryOp(TokenKind k) const { return precedence(k) >= 0; }

std::optional<std::unique_ptr<Expr>> Parser::parseInitializer() {
  if (cur_.kind == TokenKind::LBrace) {
    SourceLocation l = cur_.loc;
    advance();
    std::vector<InitElem> elems;
    if (cur_.kind != TokenKind::RBrace) {
      while (true) {
        std::vector<Designator> designators;
        SourceLocation elemLoc = cur_.loc;
        bool hasDesignator = false;
        while (cur_.kind == TokenKind::Dot || cur_.kind == TokenKind::LBracket) {
          hasDesignator = true;
          if (cur_.kind == TokenKind::Dot) {
            SourceLocation dLoc = cur_.loc;
            advance();
            if (!expect(TokenKind::Identifier, "member name")) return std::nullopt;
            std::string name = cur_.text;
            advance();
            designators.push_back(Designator::fieldName(dLoc, std::move(name)));
            continue;
          }
          SourceLocation dLoc = cur_.loc;
          advance();
          if (cur_.kind != TokenKind::IntegerLiteral) {
            diags_.error(cur_.loc, "expected integer literal in array designator");
            return std::nullopt;
          }
          size_t idx = static_cast<size_t>(std::stoll(cur_.text));
          advance();
          if (!expect(TokenKind::RBracket, "']'")) return std::nullopt;
          advance();
          designators.push_back(Designator::arrayIndex(dLoc, idx));
        }
        if (hasDesignator) {
          if (!expect(TokenKind::Assign, "'='")) return std::nullopt;
          advance();
        }
        auto elem = parseInitializer();
        if (!elem) return std::nullopt;
        if (!hasDesignator) elemLoc = (*elem)->loc;
        elems.emplace_back(elemLoc, std::move(designators), std::move(*elem));
        if (cur_.kind == TokenKind::Comma) {
          advance();
          if (cur_.kind == TokenKind::RBrace) break;
          continue;
        }
        break;
      }
    }
    if (!expect(TokenKind::RBrace, "'}'")) return std::nullopt;
    advance();
    return std::make_unique<InitListExpr>(l, std::move(elems));
  }
  return parseAssignmentExpr();
}

std::optional<std::unique_ptr<Expr>> Parser::parseAssignmentExpr() {
  auto lhs = parseConditionalExpr();
  if (!lhs) return std::nullopt;

  auto isAssignOp = [&](TokenKind k) {
    switch (k) {
      case TokenKind::Assign:
      case TokenKind::PlusAssign:
      case TokenKind::MinusAssign:
      case TokenKind::StarAssign:
      case TokenKind::SlashAssign:
      case TokenKind::PercentAssign:
      case TokenKind::AmpAssign:
      case TokenKind::PipeAssign:
      case TokenKind::CaretAssign:
      case TokenKind::LessLessAssign:
      case TokenKind::GreaterGreaterAssign:
        return true;
      default:
        return false;
    }
  };

  if (isAssignOp(cur_.kind)) {
    bool ok = dynamic_cast<VarRefExpr*>(lhs->get()) != nullptr;
    if (!ok) {
      if (auto* un = dynamic_cast<UnaryExpr*>(lhs->get())) ok = (un->op == TokenKind::Star);
    }
    if (!ok) {
      if (dynamic_cast<SubscriptExpr*>(lhs->get())) ok = true;
    }
    if (!ok) {
      if (dynamic_cast<MemberExpr*>(lhs->get())) ok = true;
    }
    if (!ok) {
      diags_.error(cur_.loc, "expected identifier on left-hand side of assignment");
      return std::nullopt;
    }

    TokenKind op = cur_.kind;
    SourceLocation assignLoc = cur_.loc;
    advance();
    auto rhs = parseAssignmentExpr(); // right associative
    if (!rhs) return std::nullopt;

    return std::make_unique<AssignExpr>(assignLoc, op, std::move(*lhs), std::move(*rhs));
  }

  return lhs;
}

std::optional<std::unique_ptr<Expr>> Parser::parseConditionalExpr() {
  auto parseMultiplicative = [&]() -> std::optional<std::unique_ptr<Expr>> {
    auto lhs = parseUnary();
    if (!lhs) return std::nullopt;
    while (cur_.kind == TokenKind::Star || cur_.kind == TokenKind::Slash ||
           cur_.kind == TokenKind::Percent) {
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
    auto parseShift = [&]() -> std::optional<std::unique_ptr<Expr>> {
      auto lhs = parseAdditive();
      if (!lhs) return std::nullopt;
      while (cur_.kind == TokenKind::LessLess || cur_.kind == TokenKind::GreaterGreater) {
        TokenKind op = cur_.kind;
        SourceLocation l = (*lhs)->loc;
        advance();
        auto rhs = parseAdditive();
        if (!rhs) return std::nullopt;
        lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
      }
      return lhs;
    };

    auto lhs = parseShift();
    if (!lhs) return std::nullopt;
    while (cur_.kind == TokenKind::Less || cur_.kind == TokenKind::LessEqual ||
           cur_.kind == TokenKind::Greater || cur_.kind == TokenKind::GreaterEqual) {
      TokenKind op = cur_.kind;
      SourceLocation l = (*lhs)->loc;
      advance();
      auto rhs = parseShift();
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

  auto parseBitAnd = [&]() -> std::optional<std::unique_ptr<Expr>> {
    auto lhs = parseEquality();
    if (!lhs) return std::nullopt;
    while (cur_.kind == TokenKind::Amp) {
      TokenKind op = cur_.kind;
      SourceLocation l = (*lhs)->loc;
      advance();
      auto rhs = parseEquality();
      if (!rhs) return std::nullopt;
      lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
    }
    return lhs;
  };

  auto parseBitXor = [&]() -> std::optional<std::unique_ptr<Expr>> {
    auto lhs = parseBitAnd();
    if (!lhs) return std::nullopt;
    while (cur_.kind == TokenKind::Caret) {
      TokenKind op = cur_.kind;
      SourceLocation l = (*lhs)->loc;
      advance();
      auto rhs = parseBitAnd();
      if (!rhs) return std::nullopt;
      lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
    }
    return lhs;
  };

  auto parseBitOr = [&]() -> std::optional<std::unique_ptr<Expr>> {
    auto lhs = parseBitXor();
    if (!lhs) return std::nullopt;
    while (cur_.kind == TokenKind::Pipe) {
      TokenKind op = cur_.kind;
      SourceLocation l = (*lhs)->loc;
      advance();
      auto rhs = parseBitXor();
      if (!rhs) return std::nullopt;
      lhs = std::make_unique<BinaryExpr>(l, op, std::move(*lhs), std::move(*rhs));
    }
    return lhs;
  };

  auto parseLogicalAnd = [&]() -> std::optional<std::unique_ptr<Expr>> {
    auto lhs = parseBitOr();
    if (!lhs) return std::nullopt;
    while (cur_.kind == TokenKind::AmpAmp) {
      TokenKind op = cur_.kind;
      SourceLocation l = (*lhs)->loc;
      advance();
      auto rhs = parseBitOr();
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

  auto lhs = parseLogicalOr();
  if (!lhs) return std::nullopt;

  if (cur_.kind == TokenKind::Question) {
    SourceLocation qLoc = cur_.loc;
    advance();
    auto thenExpr = parseAssignmentExpr();
    if (!thenExpr) return std::nullopt;

    if (!expect(TokenKind::Colon, "':'")) return std::nullopt;
    advance();

    auto elseExpr = parseConditionalExpr(); // right associative
    if (!elseExpr) return std::nullopt;

    return std::make_unique<TernaryExpr>(qLoc, std::move(*lhs), std::move(*thenExpr),
                                         std::move(*elseExpr));
  }

  return lhs;
}

std::optional<std::unique_ptr<Expr>> Parser::parseExpr() {
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
