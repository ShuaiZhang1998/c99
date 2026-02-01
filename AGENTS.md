# Repository Guidelines

## Project Structure & Module Organization

- `src/`: C++17 compiler frontend (lexer, parser, sema, codegen).
- `include/`: Minimal C99 runtime headers used by tests and examples.
- `runtime/`: Minimal libc-like runtime implementation compiled and linked by the driver.
- `tests/`: Regression tests (`ok/` and `err/`) plus `run.sh` test runner.
- `tools/`: Helper scripts/utilities (if any).
- `build/`: CMake build output (ignored in source control).

## Build, Test, and Development Commands

- Build (clean):
  ```sh
  rm -rf build
  cmake -S . -B build -DLLVM_DIR=$(llvm-config --cmakedir)
  cmake --build build -j
  ```
  Produces `build/c99cc`.
- Run tests:
  ```sh
  ./tests/run.sh
  ```
  Compiles and executes `tests/ok/*.c` and checks diagnostics for `tests/err/*.c`.
- Compile a sample program:
  ```sh
  ./build/c99cc hello.c -I include -o hello
  ```

## Coding Style & Naming Conventions

- C++ style: 2-space indentation, K&R braces, `namespace c99cc`.
- Filenames are lowercase with underscores (e.g., `parser.cpp`).
- Prefer clear, direct names (`parseIntLiteralToken`, `emitExpr`).
- No formatter or linter is enforced; keep changes consistent with existing style.

## Testing Guidelines

- Add/adjust tests under `tests/ok/` for successful compilation and `tests/err/` for expected diagnostics.
- Use the file-level comments in tests (`// EXPECT:` or `// ERROR:`) to specify expected behavior.
- This project follows TDD: add tests first when introducing features or fixing bugs.

## Commit & Pull Request Guidelines

- Commit messages in history are short and direct (e.g., `fix scanf read`, `more features`).
- Keep commits focused; include tests for behavior changes.
- PRs (if used) should describe the change, list tests run, and note any runtime/header impacts.

## Agent-Specific Notes

- Runtime headers in `include/` are intentionally minimal and must stay compatible with `runtime/` implementations.
- Avoid relying on system libc macros in runtime sources; prefer project headers with `-I include`.
