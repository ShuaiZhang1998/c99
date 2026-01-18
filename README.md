# **c99cc（里程碑 1）**

一个从零开始实现的 C99 编译器项目（当前实现一个**可运行、可测试、结构清晰**的子集）。

- 前端：C++17（Lexer / Parser / Sema / CodeGen）
- 后端：LLVM（生成目标文件 .o）
- 链接：clang（driver 调用，后续计划切到 lld）

## **已实现功能（Milestone 1）**

### **语言子集**

- 多翻译单元（多个 `.c` 输入统一链接）
- 多个函数与原型声明
- 全局变量定义
- 基础标准库头文件（最小）：`stddef.h` / `stdint.h` / `stdbool.h` / `string.h` / `stdlib.h`（需 `-I include`）
- 极简预处理器：`#include`（`"..."` 与 `<...>`，支持 `-I`/`-isystem` 搜索路径）、`#define`（含可变参数、`#`/`##`）、`#undef`、`#ifdef/#ifndef/#if/#elif/#else/#endif`，以及内置宏 `__FILE__`/`__LINE__`/`__DATE__`/`__TIME__`

### **类型系统（当前支持）**

- `int`
- `char`
- `short`
- `long`
- `long long`
- `unsigned`（`unsigned char/short/int/long/long long`）
- `float`
- `double`
- 指针（多级）：`int*`, `int**`, `void*`
- 数组（含多维）
- `struct`（定义、变量、成员访问、按值传参/返回/赋值、比较）

### **函数**

```
int f(int x, int* p);
int f(int x, int* p) { return x + p[0]; }
```

- 支持原型与定义
- 参数可省略名称（原型）

### **变量声明**

```
int x;
int y = 3;
int z = x + 4;
int a[2][3];
```

### **初始化（当前支持）**

- 标量初始化：`int x = 3;`, `float f = 1.5f;`
- 数组初始化：`int a[3] = {1, 2, 3};`, `int b[] = {1, 2};`
- 设计化初始化：`int a[4] = {[2] = 7};`, `struct S s = {.b = 2, .a = 1};`
- 字符数组字符串初始化：`char s[] = "hi";`, `char t[] = {"hi"};`
- 结构体初始化列表（含嵌套字段、数组字段）：

```
struct Inner { int x; };
struct Outer { struct Inner in; int arr[2]; };
struct Outer o = {{1}, {2, 3}};
```

> 说明：数组初始化目前仅支持顺序初始化，尚不支持设计化初始化。

### **表达式**

#### **基本表达式**

```
123        // 整数字面量
1.5f       // 浮点字面量
'A'        // 字符字面量
"hi"       // 字符串字面量
"a" "b"    // 字符串拼接
x          // 变量引用
(expr)     // 括号分组
```

> 字面量当前仅支持十进制；整数无 U/L/LL 等后缀，浮点仅支持十进制与科学计数法及 `f/F` 后缀。

#### **一元运算**

```
+x
-x
!x
~x
&x
*p
++x
x++
```

#### **二元算术运算（左结合 + 正确优先级）**

```
+  -  *  /  %
```

- 支持整数与浮点数混合运算（按常规转换）

#### **位运算与移位（整数）**

```
&  |  ^  <<  >>
```

#### **比较运算（结果为 0 / 1）**

```
==  !=  <  <=  >  >=
```

- 支持指针比较（含与 `0/NULL` 比较）
- 支持整数与浮点比较
- 支持结构体按值比较（`==`, `!=`）

#### **逻辑运算（短路）**

```
&&
||
```

- 严格短路求值
- RHS 仅在需要时求值

#### **赋值表达式（右结合）**

```
a = b = c;
```

#### **复合赋值**

```
+=  -=  *=  /=  %=  &=  |=  ^=  <<=  >>=
```

#### **逗号表达式**

```
(a = 1, b = 2, a + b)
```

#### **数组与指针**

```
int a[2];
int *p = a;     // 数组衰减
p[1] = 3;       // 下标
*(p + 1) = 3;   // 指针算术
```

#### **结构体成员访问**

```
struct S { int a; int b; };
struct S s; s.a = 1; s.b = 2;
struct S *p = &s; p->a = 3;
```

### **控制流语句**

#### **if / else**

```
if (cond) stmt;
if (cond) stmt1 else stmt2;
```

#### **while / do-while / for**

```
while (cond) stmt;

do { stmt; } while (cond);

for (init; cond; inc) stmt;
```

- `for` 支持：
  - init 为空 / 声明语句 / 任意表达式（含逗号表达式）
  - cond 为空时视为真
  - inc 支持任意表达式（含逗号表达式）
  - for 自身引入独立作用域

#### **switch / case / default**

```
switch (x) {
  case 1: ...; break;
  default: ...;
}
```

#### **break / continue**

```
break;
continue;
```

### **语义分析（Sema）**

在进入 CodeGen 之前执行最小但严格的语义检查。

- 使用未声明变量 / 未声明函数
- 重复声明（同一作用域）
- 作用域规则（遮蔽、泄漏）
- `break/continue` 位置检查
- 参数/返回类型一致性与原型冲突检测
- 指针与数组的基础合法性检查
- 结构体字段查找与类型一致性检查
- 初始化列表的结构/数组维度检查

诊断信息包含：
- 文件名
- 行号 / 列号（基于 token 位置）

### **代码生成（LLVM）**

- 在内存中生成 LLVM IR
- 目标平台：当前已在 **AArch64 (arm64)** 上验证
- 使用 clang 链接生成最终可执行文件

## **构建（Build）**

### **依赖**

- CMake >= 3.20
- LLVM（当前验证 LLVM 14）
- clang（用于链接）

### **构建步骤**

```
rm -rf build
cmake -S . -B build -DLLVM_DIR=$(llvm-config --cmakedir)
cmake --build build -j
```

## **快速测试（手动）**

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
echo $?
# 期望输出：3
```

## **编译方式**

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

多文件示例：

```
cat > a.c << 'EOS'
int add2(int a, int b) { return a + b; }
EOS

cat > b.c << 'EOS'
int add2(int a, int b);
int main() { return add2(3, 4); }
EOS

./build/c99cc a.c b.c -o ab
./ab
echo $?
# 期望输出：7
```

仅编译不链接（输出 `.o`）：

```
./build/c99cc -c a.c
./build/c99cc -c b.c
```

## **自动化测试**

项目自带最小但完整的回归测试系统：

### **测试分类**

- tests/ok/*.c
  - 应当编译成功
  - 程序退出码需匹配：

```
// EXPECT: <整数>
```

- tests/err/*.c
  - 应当编译失败
  - stderr 中需包含：

```
// ERROR: <关键子串>
```

### **运行测试**

```
./tests/run.sh
```

当前测试覆盖：

- 表达式优先级 / 结合性
- 赋值 / 逗号表达式
- 一元 / 二元 / 逻辑 / 位运算 / 移位 / 复合赋值
- 指针、数组、多维数组与数组衰减
- 结构体定义、成员访问、赋值、初始化、比较
- block 作用域与遮蔽
- if / while / do-while / for / switch
- break / continue
- 语义错误（未声明、重复定义、作用域泄漏）
- 常见语法错误（缺分号 / 括号 / 非法字符）

## **已知限制（尚未实现）**

- 数值字面量不支持十六进制/八进制、整数 U/L/LL 后缀、十六进制浮点
- 整数提升与常规算术转换规则仍是简化版（与完整 C99 存在差异）
- 字符/字符串字面量仅支持基础转义（如 `\\n`/`\\t`/`\\r`/`\\0`/`\\\\`/`\\'`/`\\\"`），不支持八进制/十六进制转义
- 预处理器能力有限：支持 `#include "..."` 与 `<...>`（搜索顺序：当前文件目录、`-I`、`-isystem`）、对象宏与函数宏（含可变参数、`#`/`##`）、内置宏 `__FILE__`/`__LINE__`/`__DATE__`/`__TIME__`、`#ifdef/#ifndef/#if/#elif/#else/#endif`；`#if` 支持整数/宏数值/`defined` 与 `!`、`~`、`+`、`-`、`*`、`/`、`%`、移位、位运算、比较、`&&`、`||`
- 无 `union`、位域、`_Bool`、`long double`
- `typedef`/`enum`/显式类型转换/`sizeof` 已支持，但规则仍较简化
- 仅支持 `==`/`!=` 的结构体比较（无排序比较）
- 无数组对象的独立初始化列表（但结构体内数组字段可初始化）
- 不支持结构体在块内定义（仅顶层）
- 错误恢复能力有限

## **如果目标是编译大多数 C99 项目，最重要的缺口（当前状态）**

- 标准库与头文件体系（除了最小 `stddef.h/stdint.h/stdbool.h` 外，仍缺常见 libc 头文件 + 对应符号）
- 预处理器更完整：include guard、内置宏更全、宏展开/token 规则一致
- 类型与初始化语义：更完整的类型提升/转换、初始化语义细节完善

## **尚缺的大块能力（后续扩展方向）**

- 预处理器：include guard 语义更完整、内置宏更齐、宏展开与 token 规则更严格
- 多翻译单元与链接模型
- 类型与转换：完整整数提升/常规算术转换、显式转换、`sizeof` 语义完善
- 更多类型：`union`、位域、`_Bool`、`long double`
- 字面量：整数 U/L/LL 后缀、十六/八进制、十六进制浮点、转义更完整
- 初始化：字符串初始化数组更完整
- 控制流与语义：`goto`/label、`switch` case 表达式与类型规则更完整
- 作用域与存储期：块内 `struct` 定义、`extern`/`static` 等
- 错误恢复与诊断增强

## **目标是编译一个经典而简单的 C99 项目，还缺什么（当前状态）**

- 标准库与头文件体系：`stdio.h`/`stdlib.h`/`string.h` 等常用头文件与符号
- 预处理器完善：完整 include guard、宏展开细节一致性
- 字面量与转义：十六/八进制整数、更多转义序列
- 类型与初始化语义：更完整的整数提升/常规算术转换

