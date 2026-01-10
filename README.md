

# c99cc（里程碑 1）

一个从零开始实现的 C99 编译器项目（当前仅实现一个可运行的最小子集）。

- 前端：C++17（Lexer / Parser / Sema / CodeGen）
- 后端：LLVM（生成目标文件 .o）
- 链接：clang（driver 调用，后续计划切到 lld）

---

## 已实现功能（Milestone 1）

### 语言子集

当前仅支持**单个翻译单元**、**单个函数定义**：

- 函数定义：
  - `int <name>() { ... }`

- 函数体语句（多条）：
  - 局部变量声明：`int x;`
  - 赋值语句：`x = <expr>;`
  - 返回语句：`return <expr>;`

- 表达式：
  - 整数字面量：`123`
  - 变量引用：`x`
  - 二元运算（左结合 + 优先级）：
    - `+ - * /`
  - 括号分组：`( ... )`

### 语义分析（Sema）

在进入 CodeGen 之前做最小语义检查，支持诊断：

- `use of undeclared identifier 'x'`（使用未声明变量）
- `assignment to undeclared identifier 'x'`（给未声明变量赋值）
- `redefinition of 'x'`（重复声明）

诊断信息包含 filename / line / column（基于 token 位置）。

### 代码生成（LLVM）

- 生成 LLVM IR（内存中）
- 局部变量 lowering：
  - `alloca`（放在 entry block）
  - `store` / `load`
- 通过 LLVM TargetMachine 生成本机目标文件（AArch64 可用）
- 使用 `clang` 链接生成可执行文件

---

## 构建（Build）

### 依赖

- CMake >= 3.20
- LLVM（当前验证 LLVM 14）
- clang（用于链接）

### 配置 + 编译

```bash
rm -rf build
cmake -S . -B build -DLLVM_DIR=$(llvm-config --cmakedir)
cmake --build build -j
```





------





## **快速测试（手动）**



```
cat > hello.c << 'EOF'
int main() {
  int x;
  x = 3;
  return x + 4;
}
EOF

./build/c99cc hello.c -o hello
./hello
echo $?
# 期望：7
```



------





## **自动化测试（推荐）**





本项目使用 tests/run.sh 做最小回归测试：



- tests/ok/*.c：应当编译成功，并运行得到指定退出码

  

  - 文件第一行写：// EXPECT: <整数>

  

- tests/err/*.c：应当编译失败，并在 stderr 中包含指定子串

  

  - 文件第一行写：// ERROR: <关键子串>

  





运行：

```
./tests/run.sh
```



------





## **已知限制（尚未实现）**





- 无嵌套作用域（仅函数体一层符号表）
- 无初始化声明：int x = 3;（计划下一里程碑做）
- 无函数参数/调用
- 无控制流：if/while/for
- 无除 int 外的类型
- 无一元运算、比较运算、逻辑运算（含短路）
- 无预处理器：#include / 宏 / 条件编译
- 无多翻译单元 / 更完整的链接选项
- 错误恢复能力有限





------





## **下一步计划**





- 支持初始化声明：int x = <expr>;
- 支持一元运算：+ - ! ~
- 支持比较与逻辑短路：== != < <= > >= && ||
- 支持控制流：if/else、while、for
- 支持函数参数与调用
- 支持 block scope + 符号表栈
- 逐步补全 C99 类型系统与转换规则
- 实现预处理器
