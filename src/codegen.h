#pragma once
#include <memory>
#include <string>
#include "parser.h"

namespace llvm {
class LLVMContext;
class Module;
}

namespace c99cc {

class CodeGen {
public:
  static std::unique_ptr<llvm::Module> emitLLVM(
      llvm::LLVMContext& ctx,
      const AstTranslationUnit& tu,
      const std::string& moduleName);
};

} // namespace c99cc
