# c99cc (Milestone 1)

一个从零开始实现的 C99 编译器项目，目标是实现**可运行、可测试、结构清晰**的 C99 子集，适合学习与实验。

- 前端：C++17（Lexer / Parser / Sema / CodeGen）
- 后端：LLVM（生成目标文件 .o）
- 链接：clang（driver 调用）

## 功能概览

### 语言与预处理器

- 多翻译单元（多个 `.c` 输入统一链接）
- 函数定义与原型声明
- 全局变量定义
- 预处理器（子集）：
  - `#include`（`"..."` 与 `<...>`，支持 `-I`/`-isystem` 搜索路径）
  - `#define`（对象宏 / 函数宏，含可变参数、`#`/`##`）
  - `#undef`、`#ifdef/#ifndef/#if/#elif/#else/#endif`
  - 内置宏：`__FILE__` / `__LINE__` / `__DATE__` / `__TIME__`

### 类型系统

- 基本类型：`int` / `char` / `short` / `long` / `long long`
- 无符号：`unsigned char/short/int/long/long long`
- 浮点：`float` / `double`
- 指针（多级）：`int*`, `int**`, `void*`
- 数组（含多维）
- `struct`（定义、成员访问、按值传参/返回/赋值、比较）
- `typedef` 与 `enum`

### 表达式与语句

- 字面量：整数（十进制）、浮点（十进制与科学计数法、`f/F`）、字符与字符串
- 运算符：一元/二元算术、位运算、移位、比较、逻辑（短路）、逗号、赋值与复合赋值
- 显式类型转换与 `sizeof`
- 指针算术与数组下标
- 结构体成员访问（`.` / `->`）
- 控制流：`if/else`、`while`、`do-while`、`for`、`switch`、`break/continue`

### 初始化

- 标量初始化
- 数组初始化（顺序与设计化）
- 字符数组字符串初始化
- 结构体初始化（含嵌套字段与数组字段）

### 语义分析（Sema）

- 未声明符号、重复声明、作用域规则
- `break/continue` 位置检查
- 参数/返回类型一致性与原型冲突检测
- 指针/数组/结构体的基础合法性检查
- 初始化列表的结构与维度检查

诊断信息包含：文件名、行号与列号（基于 token 位置）。

### 代码生成与链接

- 内存中生成 LLVM IR
- 目标平台：已在 AArch64 (arm64) 上验证
- 使用 clang 进行链接

### 标准库与运行时（最小）

- 头文件（需 `-I include`）：`stddef.h` / `stdint.h` / `stdbool.h` / `string.h` / `stdlib.h` / `stdio.h` / `ctype.h`
- `printf`（最小实现）：
  - 支持 `%d/%i/%c/%s/%f/%%`
  - 支持最小宽度
  - 支持精度：`%f`（小数位）、`%s`（截断）
  - 不支持对齐标志、填充、符号、进制等扩展
- `putchar/puts`
- `fopen/fclose/fread/fwrite/fprintf/sprintf/snprintf`（基础文件 I/O 与格式化 I/O）
- `scanf/sscanf`（最小实现：`%d/%u/%x/%f/%s/%c`、`%%`、宽度与空白匹配）
- `stdin/stdout/stderr`（在 `stdio.h` 中通过访问器宏提供）
- `malloc/calloc/realloc/free`（最小实现：POSIX 使用 `mmap`，Windows 使用 `VirtualAlloc`；`free` 可释放整块）
- `stdlib.h`（最小实现：`atoi/atol/atoll`、`abs/labs/llabs`、`div/ldiv`、`exit/abort`）
- `string.h`（最小实现：`memcpy/memmove/memset/memcmp`、`strlen/strcmp`、`strcpy/strncpy`、`strcat/strncat`）
- `ctype.h`（最小实现：`isdigit`、`isspace`）
- 运行时编译使用 `-I include`，避免系统头文件宏与本项目最小实现冲突

## 构建

### 依赖

- CMake >= 3.20
- LLVM（当前验证 LLVM 14）
- clang（用于链接）

### 构建步骤

```
rm -rf build
cmake -S . -B build -DLLVM_DIR=$(llvm-config --cmakedir)
cmake --build build -j
```

## 使用

单文件：

```
./build/c99cc hello.c -o hello
```

多文件（自动编译并链接）：

```
./build/c99cc a.c b.c -o app
```

仅编译不链接（输出 `.o`）：

```
./build/c99cc -c a.c
./build/c99cc -c b.c
```

## 示例

基础示例：

```
cat > hello.c << 'EOS'
int sum(int x, int y) { return x + y; }
int main() {
  int a[2];
  a[0] = 1; a[1] = 2;
  return sum(a[0], a[1]);
}
EOS

./build/c99cc hello.c -o hello
./hello
# 期望退出码：3
```

`printf` 示例（需要 `-I include`）：

```
cat > hello.c << 'EOS'
#include <stdio.h>
int main() {
  printf("sum=%d\n", 3);
  return 0;
}
EOS

./build/c99cc hello.c -I include -o hello
./hello
# 期望输出：sum=3
```

带宽度/精度的 `printf` 示例：

```
cat > hello.c << 'EOS'
#include <stdio.h>
int main() {
  printf("%4d %.2f\n", 12, 1.234);
  printf("%.3s\n", "abcdef");
  return 0;
}
EOS

./build/c99cc hello.c -I include -o hello
./hello
# 期望输出："  12 1.23" 与 "abc"
```

## 测试

项目自带最小但完整的回归测试系统：

- `tests/ok/*.c`：应当编译成功，并匹配退出码
  - `// EXPECT: <整数>`
- `tests/err/*.c`：应当编译失败，并匹配错误子串
  - `// ERROR: <关键子串>`

运行测试：

```
./tests/run.sh
```

## 已知限制与缺口（面向常见 C99 项目）

- 数值字面量：不支持十六/八进制、整数 U/L/LL 后缀、十六进制浮点
- 转义序列：仅支持基础转义（如 `\n`/`\t`/`\r`/`\0`/`\\`/`\'`/`\"`）
- 类型与转换：整数提升与常规算术转换为简化版
- 预处理器：不支持 `#pragma once`，宏展开与 token 规则不完全一致于标准
- 类型系统：无 `union`、位域、`_Bool`、`long double`
- 作用域与存储期：`extern`/`static` 等存储类尚未覆盖
- 标准库：仅提供最小头文件与极简 `printf`，无完整 libc 实现
- 内存分配：`malloc/calloc/realloc/free` 为最小实现，不支持碎片整理与复用策略
- 诊断与错误恢复：仍较有限

## 接下来优先完善的 C 运行时（面向简单 C99 项目）

- `stdio.h`：继续扩展 `scanf/sscanf` 规格与更完整的文件/缓冲 I/O
- `stdlib.h`：`rand/srand`、`strtol/strtoul/strtod`、`qsort/bsearch`、`getenv`
- `string.h`：`strchr/strrchr/strstr/strtok`、`memchr`
- `math.h`：`sqrt/pow/sin/cos` 等基础函数
- `time.h` / `errno.h` / `assert.h` / `signal.h`
