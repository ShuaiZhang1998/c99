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

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: c99cc <input.c> [-o <output>]\n";
    return 1;
  }

  std::string inputPath = argv[1];
  std::string outPath = "a.out";

  for (int i = 2; i < argc; i++) {
    std::string a = argv[i];
    if (a == "-o" && i + 1 < argc) {
      outPath = argv[++i];
    } else {
      std::cerr << "unknown arg: " << a << "\n";
      return 1;
    }
  }

  std::string source = readFileOrDie(inputPath);

  c99cc::Diagnostics diags;
  c99cc::Lexer lex(source, diags);
  c99cc::Parser parser(lex, diags);

  auto tuOpt = parser.parse();
  if (!tuOpt || diags.hasError()) {
    diags.printAll(inputPath, source);
    return 1;
  }

  // Optional but recommended: ensure there is a main() function
  bool hasMain = false;
  for (const auto& fn : tuOpt->functions) {
    if (fn.name == "main") {
      hasMain = true;
      break;
    }
  }
  if (!hasMain) {
    std::cerr << "error: no 'main' function defined\n";
    return 1;
  }

  c99cc::Sema sema(diags);
  if (!sema.run(*tuOpt) || diags.hasError()) {
    diags.printAll(inputPath, source);
    return 1;
  }

  llvm::LLVMContext ctx;
  auto mod = c99cc::CodeGen::emitLLVM(ctx, *tuOpt, inputPath);

  std::string objPath = outPath + ".o";
  writeObjOrDie(*mod, objPath);

  std::string cmd = "clang \"" + objPath + "\" -o \"" + outPath + "\"";
  int rc = std::system(cmd.c_str());
  if (rc != 0) {
    std::cerr << "link failed (cmd=" << cmd << ")\n";
    return 1;
  }

  return 0;
}
