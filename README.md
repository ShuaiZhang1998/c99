# c99cc（里程碑 1）

一个从零开始实现的 C99 编译器项目（当前实现一个**可运行、可测试、结构清晰**的最小子集）。

- 前端：C++17（Lexer / Parser / Sema / CodeGen）
- 后端：LLVM（生成目标文件 .o）
- 链接：clang（driver 调用，后续计划切到 lld）

---

## 已实现功能（Milestone 1）

### 语言子集

当前仅支持**单个翻译单元**、**单个函数定义**：

#### 函数定义

```c
int <name>() { ... }
```

#### **函数体语句**

- 局部变量声明：

```
int x;
int y = 3;
int z = x + 4;
```

- 赋值语句：

```
x = <expr>;
```

- 返回语句：

```
return <expr>;
```

#### **表达式**

- 整数字面量：

```
123
```

- 变量引用：

```
x
```

- 一元运算：

```
+x  -x  !x  ~x
```

- 二元运算（**左结合 + 正确优先级**）：

```
+  -  *  /
```

- 括号分组：

```
( ... )
```

### **语义分析（Sema）**

在进入 CodeGen 之前执行最小语义检查，保证 AST 在语义上是“可生成代码的”。

已实现的诊断包括：

- 使用未声明变量：

```
use of undeclared identifier 'x'
```

- 给未声明变量赋值：

```
assignment to undeclared identifier 'x'
```

- 重复声明变量：

```
redefinition of 'x'
```

- 初始化声明规则：
  - int x = x + 1; 

诊断信息包含：

- 文件名
- 行号 / 列号（基于 token 位置）

### **代码生成（LLVM）**

- 在内存中生成 LLVM IR
- 局部变量 lowering：
  - 在 entry block 中使用 alloca
  - 使用 store / load
- 支持初始化声明：

```
%x = alloca i32
store i32 <init>, i32* %x
```

- 一元运算 lowering：
  - -x → sub 0, x / neg
  - ~x → not
  - !x → icmp + zext（生成 0/1）
- 通过 LLVM TargetMachine 生成本机目标文件
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
  int x = 3;
  return x + 4;
}
EOF

./build/c99cc hello.c -o hello
./hello
echo $?
# 期望输出：7
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
- 初始化声明
- 一元运算
- 变量声明 / 赋值
- 语义错误（未声明、重复定义）
- 常见语法错误（缺分号 / 括号 / 非法字符）

## **已知限制（尚未实现）**

- 无嵌套作用域（仅函数体一层符号表）
- 无函数参数 / 函数调用
- 无控制流语句：if / while / for
- 无比较 / 逻辑运算（== != < > && ||）
- 仅支持 int 类型
- 无类型转换规则
- 无预处理器（#include / 宏 / 条件编译）
- 无多翻译单元
- 错误恢复能力有限

## **下一步计划（Milestone 2）**

- 比较运算：== != < <= > >=
- 逻辑运算与短路：&& ||
- 控制流：if / else、while、for
- 函数参数与调用
- block scope + 符号表栈
- 逐步补全 C99 类型系统与转换
- 预处理器实现




