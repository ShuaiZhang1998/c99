# **c99cc（里程碑 1）**

一个从零开始实现的 C99 编译器项目（当前实现一个**可运行、可测试、结构清晰**的子集）。

- 前端：C++17（Lexer / Parser / Sema / CodeGen）
- 后端：LLVM（生成目标文件 .o）
- 链接：clang（driver 调用，后续计划切到 lld）

## **已实现功能（Milestone 1）**

### **语言子集**

- 单个翻译单元
- 多个函数与原型声明
- 全局变量定义

### **类型系统（当前支持）**

- `int`
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

- 标量初始化：`int x = 3;`
- 结构体初始化列表（含嵌套字段、数组字段）：

```
struct Inner { int x; };
struct Outer { struct Inner in; int arr[2]; };
struct Outer o = {{1}, {2, 3}};
```

> 说明：当前仍不支持**独立数组对象**的初始化列表（但结构体中的数组字段可初始化）。

### **表达式**

#### **基本表达式**

```
123        // 整数字面量
x          // 变量引用
(expr)     // 括号分组
```

#### **一元运算**

```
+x
-x
!x
~x
&x
*p
```

#### **二元算术运算（左结合 + 正确优先级）**

```
+  -  *  /
```

#### **比较运算（结果为 0 / 1）**

```
==  !=  <  <=  >  >=
```

- 支持指针比较（含与 `0/NULL` 比较）
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
- 一元 / 二元 / 逻辑运算
- 指针、数组、多维数组与数组衰减
- 结构体定义、成员访问、赋值、初始化、比较
- block 作用域与遮蔽
- if / while / do-while / for / switch
- break / continue
- 语义错误（未声明、重复定义、作用域泄漏）
- 常见语法错误（缺分号 / 括号 / 非法字符）

## **已知限制（尚未实现）**

- 仅支持 `int` 与指针/数组/struct 子集（无 `char/float/double/long`）
- 无显式类型转换、`sizeof`、`typedef`、`enum`、`union`、位域
- 仅支持 `==`/`!=` 的结构体比较（无排序比较）
- 无数组对象的独立初始化列表（但结构体内数组字段可初始化）
- 不支持结构体在块内定义（仅顶层）
- 无预处理器（#include / 宏 / 条件编译）
- 无多翻译单元
- 错误恢复能力有限

## **下一步计划（Milestone 2）**

- 更完整的 C99 类型系统与转换规则
- 更丰富的初始化语法（含数组/设计化初始化）
- 更健壮的错误恢复
- IR 层优化 / SSA 改进
