# My RISC-V OS Project

RISC-V OS - 实验一：最小化内核启动
这是一个从零开始构建的、极其精简的 RISC-V 操作系统内核。本次实验的目标是搭建一个能在 QEMU 模拟器上成功引导，并向串口输出 "Hello World" 的最小化系统。

## 功能实现
本项目实现了一个操作系统最核心的启动流程：

- 汇编入口 (entry.S): 设置初始的栈指针，为 C 语言环境做准备，并清零 .bss 段。

- 链接脚本 (kernel.ld): 定义了内核代码在内存中的布局，设定了 0x80000000 作为内核的起始地址，与 xv6 保持一致。

- 串口 (UART) 驱动 (uart.c): 一个简单的轮询式 UART 驱动，用于向 QEMU 的串口发送字符。

- C 主函数 (main.c): 在汇编代码完成初始化后，跳转到此 C 函数。它调用 UART 驱动打印信息，然后使系统进入休眠状态。

## 环境要求
在开始之前，请确保您已经安装了 RISC-V 交叉编译工具链和 QEMU 模拟器。

交叉编译工具链: riscv64-unknown-elf-gcc, riscv64-unknown-elf-ld, 等。

模拟器: qemu-system-riscv64

如何构建和运行
本项目使用 Makefile 进行自动化管理。

编译内核
在 ex_1 目录下，执行以下命令来编译所有源代码并链接生成内核文件 kernel.elf。

```Bash

make
```
或者

```Bash

make all
```
在 QEMU 中运行
编译成功后，执行以下命令启动 QEMU 并加载内核：

```Bash
make qemu
```
清理生成文件
若要清除所有编译生成的目标文件 (.o, .d) 和内核镜像，请执行：

```Bash

make clean
```
使用 GDB 调试
若要启动 QEMU 并等待 GDB 连接（监听在 1234 端口），请执行：

```Bash

make qemu-gdb
```
然后您可以在另一个终端窗口中使用 riscv64-unknown-elf-gdb 进行调试。

预期输出
当您成功运行 make qemu 后，您应该会在终端看到 QEMU 启动，并打印出以下信息：
```
Hello 05
```
这表明您的最小化内核已经成功启动并正确运行。

文件结构说明
```.
├── kernel/
│   ├── entry.S        # 汇编入口与早期初始化
│   ├── kernel.ld      # 链接器脚本，定义内存布局
│   ├── main.c         # C代码主函数
│   └── uart.c         # 串口驱动程序
├── Makefile         # 自动化构建脚本
└── README.md        # 本说明文件
```