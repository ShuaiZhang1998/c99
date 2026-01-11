

# **c99cc（里程碑 1）**

一个从零开始实现的 C99 编译器项目（当前实现一个**可运行、可测试、结构清晰**的最小子集）。

- 前端：C++17（Lexer / Parser / Sema / CodeGen）
- 后端：LLVM（生成目标文件 .o）
- 链接：clang（driver 调用，后续计划切到 lld）

## **已实现功能（Milestone 1）**

### **语言子集**

当前仅支持**单个翻译单元**、**单个函数定义**：

```
int <name>() { ... }
```

### **函数体语句**

#### **变量声明**

```
int x;
int y = 3;
int z = x + 4;
```

#### **赋值语句**

```
x = <expr>;
```

#### **表达式语句 / 空语句**

```
x + 1;
;
```

#### **返回语句**

```
return <expr>;
```

#### **代码块（Block）**

```
{
  int x = 1;
  x = x + 1;
}
```

- {} **引入新的作用域**
- 支持变量遮蔽（shadowing）
- 作用域退出后变量不可见

### **控制流语句**

#### **if / else**

```
if (cond) stmt;
if (cond) stmt1 else stmt2;
```

#### **while**

```
while (cond) stmt;
```

#### **do / while**

```
do {
  stmt;
} while (cond);
```

#### **for**

```
for (init; cond; inc) stmt;
```

支持：

- init：
  - 空
  - 声明语句：int i = 0;
  - 任意表达式（含逗号表达式）
- cond 为空时视为真
- inc 支持任意表达式（含逗号表达式）
- for 自身引入独立作用域

#### **break / continue**

```
break;
continue;
```

- 仅允许出现在循环内部
- 语义检查会拒绝 loop 外使用

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
```

#### **二元算术运算（左结合 + 正确优先级）**

```
+  -  *  /
```

#### **比较运算（结果为 0 / 1）**

```
==  !=  <  <=  >  >=
```

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

- 从左到右求值
- 结果为最后一个表达式的值
- 可用于：
  - 普通表达式
  - for 的 init / inc

## **语义分析（Sema）**

在进入 CodeGen 之前执行最小但严格的语义检查。

### **已实现的诊断**

- 使用未声明变量：

```
use of undeclared identifier 'x'
```

- 给未声明变量赋值：

```
assignment to undeclared identifier 'x'
```

- 重复声明（同一作用域）：

```
redefinition of 'x'
```

- block 作用域规则：
  - 内层可遮蔽外层
  - 作用域外访问报错
- break / continue 语义检查：
  - loop 外使用报错
- 初始化声明规则：

```
int x = x + 1; // ❌ use of undeclared identifier 'x'
```

### **诊断信息包含**

- 文件名
- 行号 / 列号（基于 token 位置）

## **代码生成（LLVM）**

### **基本策略**

- 在内存中生成 LLVM IR
- 单函数、i32 返回值

### **局部变量 lowering**

- 所有局部变量：
  - 在 entry block 中 alloca
  - 使用 store / load
- 作用域通过符号表栈管理

### **一元 / 二元运算**

- 一元：
  - -x → sub 0, x
  - ~x → not
  - !x → icmp + zext
- 比较：
  - icmp（i1）→ zext 到 i32
- 逗号运算：
  - 生成 lhs
  - 丢弃 lhs 结果
  - 返回 rhs 值

### **逻辑运算（短路）**

- && / ||：
  - 使用 condbr
  - phi 合并结果
  - 保证 RHS 仅在需要时生成

### **控制流 lowering**

- if / else
- while
- do / while
- for
- break / continue（通过 loop block 栈）

### **后端**

- 通过 LLVM TargetMachine 生成目标文件
- 当前已在 **AArch64 (arm64)** 上验证
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
cat > hello.c << 'EOF'
int main() {
  int sum = 0;
  for (int i = 0; i < 4; i = i + 1) {
    sum = sum + i;
  }
  return sum;
}
EOF

./build/c99cc hello.c -o hello
./hello
echo $?
# 期望输出：6
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
- block 作用域与遮蔽
- if / while / do-while / for
- break / continue
- 语义错误（未声明、重复定义、作用域泄漏）
- 常见语法错误（缺分号 / 括号 / 非法字符）

## **已知限制（尚未实现）**

- 无函数参数 / 函数调用
- 仅支持 int 类型
- 无类型转换规则
- 无 switch / case
- 无 goto
- 无预处理器（#include / 宏 / 条件编译）
- 无多翻译单元
- 错误恢复能力有限

## **下一步计划（Milestone 2）**

- 函数参数与调用
- 更完整的 C99 类型系统
- 隐式 / 显式类型转换
- switch / case
- 预处理器实现
- 更健壮的错误恢复
- IR 层优化 / SSA 改进


