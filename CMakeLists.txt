cmake_minimum_required(VERSION 3.20)
project(c99cc LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# If LLVM isn't in default path:
#   cmake -S . -B build -DLLVM_DIR=$(llvm-config --cmakedir)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

add_executable(c99cc
  tools/c99cc/main.cpp
  src/diag.cpp
  src/lexer.cpp
  src/parser.cpp
  src/sema.cpp
  src/codegen.cpp
)

# Minimal set for: LLVM IR -> Object file (native codegen)
llvm_map_components_to_libnames(LLVM_LIBS
  core
  support
  mc
  target
  analysis
  passes
  codegen
  bitwriter
  native
  nativecodegen
)

target_link_libraries(c99cc PRIVATE ${LLVM_LIBS})

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(c99cc PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_test(
  NAME c99cc_tests
  COMMAND ${CMAKE_SOURCE_DIR}/tests/run.sh
)
