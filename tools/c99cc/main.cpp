#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdlib>

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Host.h"

#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/MC/TargetRegistry.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"

#include "../../src/diag.h"
#include "../../src/preprocessor.h"
#include "../../src/lexer.h"
#include "../../src/parser.h"
#include "../../src/sema.h"
#include "../../src/codegen.h"

static std::string readFileOrDie(const std::string& path) {
  std::ifstream ifs(path);
  if (!ifs) {
    std::cerr << "failed to open: " << path << "\n";
    std::exit(1);
  }
  std::stringstream ss;
  ss << ifs.rdbuf();
  return ss.str();
}

static void writeObjOrDie(llvm::Module& module, const std::string& objPath) {
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetAsmParser();

  std::string targetTriple = llvm::sys::getDefaultTargetTriple();
  module.setTargetTriple(targetTriple);

  std::string err;
  const llvm::Target* target = llvm::TargetRegistry::lookupTarget(targetTriple, err);
  if (!target) {
    llvm::errs() << "Target lookup failed: " << err << "\n";
    std::exit(1);
  }

  llvm::TargetOptions opt;
  llvm::Optional<llvm::Reloc::Model> rm;

  std::unique_ptr<llvm::TargetMachine> tm(
      target->createTargetMachine(targetTriple, "generic", "", opt, rm));

  module.setDataLayout(tm->createDataLayout());

  std::error_code ec;
  llvm::raw_fd_ostream dest(objPath, ec, llvm::sys::fs::OF_None);
  if (ec) {
    llvm::errs() << "Could not open file: " << ec.message() << "\n";
    std::exit(1);
  }

  llvm::legacy::PassManager pm;
  if (tm->addPassesToEmitFile(pm, dest, nullptr, llvm::CGFT_ObjectFile)) {
    llvm::errs() << "TargetMachine can't emit a file of this type\n";
    std::exit(1);
  }

  pm.run(module);
  dest.flush();
}

static bool tuHasMain(const c99cc::AstTranslationUnit& tu) {
  // count both decl and def as "main exists"
  for (const auto& item : tu.items) {
    if (auto* d = std::get_if<c99cc::FunctionDecl>(&item)) {
      if (d->proto.name == "main") return true;
    } else if (auto* f = std::get_if<c99cc::FunctionDef>(&item)) {
      if (f->proto.name == "main") return true;
    }
  }
  return false;
}

static std::string replaceExtensionWithObj(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  size_t dot = path.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
    return path + ".o";
  }
  return path.substr(0, dot) + ".o";
}

static std::string createTempObjPath() {
  llvm::SmallString<128> tmp;
  std::error_code ec = llvm::sys::fs::createTemporaryFile("c99cc", "o", tmp);
  if (ec) {
    llvm::errs() << "Could not create temporary file: " << ec.message() << "\n";
    std::exit(1);
  }
  return tmp.str().str();
}

static bool compileToObject(
    const std::string& inputPath,
    const std::vector<std::string>& includePaths,
    const std::vector<std::string>& systemIncludePaths,
    const std::string& objPath,
    bool& hasMainOut) {
  std::string source = readFileOrDie(inputPath);

  c99cc::Preprocessor pp(includePaths, systemIncludePaths);
  auto preprocessed = pp.run(inputPath, source);
  if (!preprocessed) {
    return false;
  }
  source = *preprocessed;

  c99cc::Diagnostics diags;
  c99cc::Lexer lex(source, diags);
  c99cc::Parser parser(lex, diags);

  auto tuOpt = parser.parse();
  if (!tuOpt || diags.hasError()) {
    diags.printAll(inputPath, source);
    return false;
  }

  if (tuHasMain(*tuOpt)) hasMainOut = true;

  c99cc::Sema sema(diags);
  if (!sema.run(*tuOpt) || diags.hasError()) {
    diags.printAll(inputPath, source);
    return false;
  }

  llvm::LLVMContext ctx;
  auto mod = c99cc::CodeGen::emitLLVM(ctx, *tuOpt, inputPath);
  writeObjOrDie(*mod, objPath);
  return true;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr
        << "usage: c99cc <input.c>... [-o <output>] [-c] [-I <path>] [-isystem <path>]\n";
    return 1;
  }

  std::string outPath = "a.out";
  bool compileOnly = false;
  std::vector<std::string> inputPaths;
  std::vector<std::string> includePaths;
  std::vector<std::string> systemIncludePaths;

  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-o" && i + 1 < argc) {
      outPath = argv[++i];
    } else if (a == "-c") {
      compileOnly = true;
    } else if (a == "-I" && i + 1 < argc) {
      includePaths.push_back(argv[++i]);
    } else if (a == "-I") {
      std::cerr << "missing path after -I\n";
      return 1;
    } else if (a.rfind("-I", 0) == 0 && a.size() > 2) {
      includePaths.push_back(a.substr(2));
    } else if (a == "-isystem" && i + 1 < argc) {
      systemIncludePaths.push_back(argv[++i]);
    } else if (a == "-isystem") {
      std::cerr << "missing path after -isystem\n";
      return 1;
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "unknown arg: " << a << "\n";
      return 1;
    } else {
      inputPaths.push_back(a);
    }
  }

  if (inputPaths.empty()) {
    std::cerr << "error: no input files\n";
    return 1;
  }

  if (compileOnly && inputPaths.size() > 1 && outPath != "a.out") {
    std::cerr << "error: -o with -c requires a single input file\n";
    return 1;
  }

  bool hasMain = false;
  std::vector<std::string> objPaths;
  objPaths.reserve(inputPaths.size());

  for (size_t i = 0; i < inputPaths.size(); i++) {
    const std::string& inputPath = inputPaths[i];
    std::string objPath;
    if (compileOnly) {
      if (inputPaths.size() == 1 && outPath != "a.out") {
        objPath = outPath;
      } else {
        objPath = replaceExtensionWithObj(inputPath);
      }
    } else {
      objPath = createTempObjPath();
    }
    if (!compileToObject(
            inputPath, includePaths, systemIncludePaths, objPath, hasMain)) {
      return 1;
    }
    objPaths.push_back(objPath);
  }

  if (!compileOnly) {
    if (!hasMain) {
      std::cerr << "error: no 'main' function defined\n";
      return 1;
    }

    std::string cmd = "clang";
    for (const auto& obj : objPaths) {
      cmd += " \"" + obj + "\"";
    }
    cmd += " -o \"" + outPath + "\"";
    int rc = std::system(cmd.c_str());
    if (rc != 0) {
      std::cerr << "link failed (cmd=" << cmd << ")\n";
      return 1;
    }
  }

  return 0;
}
