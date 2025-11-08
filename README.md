
# My RISC-V OS Project

## RISC-V OS - 实验四：中断处理与时钟管理

这是一个从零开始构建的、逐步完善的 RISC-V 操作系统内核。本项目在实现了虚拟内存管理的基础上，迈出了关键一步：**搭建中断处理框架**。

这使得我们的内核不再是一个启动后就结束的程序，而是一个可以**持续运行、并异步响应硬件事件**的“活”系统。

本项目目前已完成：
1.  **实验一：最小化内核启动** - 搭建了 QEMU 引导和串口输出的最小系统。
2.  **实验二：内核 `printf` 与清屏** - 实现了功能丰富的 `printf` 调试工具。
3.  **实验三：页表与内存管理** - 构建了物理内存分配器和 Sv39 三级页表虚拟内存系统。
4.  **实验四：中断处理与时钟管理** - 实现了 S 模式的中断处理框架，并成功响应了来自 SBI 的周期性时钟中断。

## 功能实现

本项目在原有内存管理的基础上，新增了完整的中断处理（Trap）模块：

* **中断汇编入口 (`kernel/trap/kernelvec.S`)**:
    * 作为内核 S 模式的中断/异常唯一入口，其地址在 `trapinit` 中被设置到 `stvec` 寄存器。
    * 负责在中断发生时，**完整保存全部 32 个通用寄存器**的上下文到内核栈上。
    * 调用 C 语言总处理函数 `kerneltrap`。
    * 在 `kerneltrap` 返回后，从栈上**恢复全部 32 个寄存器**，并执行 `sret` 返回到被中断的代码。

* **中断处理 C 逻辑 (`kernel/trap/trap.c`)**:
    * `trapinit()`: 初始化 `stvec` 寄存器，指向汇编入口 `kernelvec`。
    * `timerinit()`: 通过 `w_sie` 开启 S 模式的时钟中断使能（SIE_STIE），并设置第一次时钟中断。
    * `kerneltrap()`: 作为 C 语言的中断总管，通过读取 `scause` 寄存器判断中断原因。目前，它专门处理 S 模式时钟中断（`interrupt_code == 5`），打印信息，并调用 `set_next_timer()` 重新“上弦”，实现周期性中断。

* **SBI 驱动 (`kernel/driver/sbi.c`)**:
    * 新增的底层驱动，通过 `ecall` 指令封装了 RISC-V 的 SBI（Supervisor Binary Interface）调用。
    * 目前实现了 `sbi_set_timer`，用于请求 M 模式设置下一次时钟中断。

* **CSR 寄存器 (`include/riscv.h`)**:
    * 头文件被大幅扩展，加入了 `r_sstatus`, `w_sstatus`, `r_sie`, `w_sie`, `r_scause`, `w_stvec`, `r_time` 等关键 CSR 寄存器的内联汇编读写函数，这是实现中断控制的基石。

* **内核主函数 (`kernel/main.c`)**:
    * 启动流程发生重大变化。在 `kvminithart` 启用分页之后，依次调用 `trapinit()` 和 `timerinit()`。
    * 通过 `intr_on()` 全局开启中断。
    * 最后进入 `while(1)` 无限循环，使 CPU 空转，等待中断的发生。

## 环境要求

在开始之前，请确保您已经安装了 RISC-V 交叉编译工具链和 QEMU 模拟器。

* **交叉编译工具链**: `riscv64-unknown-elf-gcc`, `riscv64-unknown-elf-ld`, 等。
* **模拟器**: `qemu-system-riscv64`

## 如何构建和运行

本项目使用 `Makefile` 进行自动化管理。

#### 编译内核

在项目根目录下，执行以下命令来编译所有源代码并链接生成内核文件 `kernel/kernel.elf`。

```zsh
make
````

#### 运行内核

执行 `make qemu` 来启动 QEMU 模拟器并运行您的内核。

```zsh
make qemu
```

您将看到系统启动，完成内存管理初始化，然后开始周期性地打印时钟中断信息。

#### 调试内核

您可以使用 `make debug` 来启动 GDB 调试会话。

```zsh
make debug
```

这会自动启动 QEMU（暂停在第一条指令）和一个 GDB 客户端连接到它，并为您加载好带符号的内核文件 `kernel/kernel.elf`。

## 文件结构说明

```
.
├── include/
│   ├── console.h      # 控制台与 printf 接口
│   ├── memory.h       # 内存管理接口
│   ├── riscv.h        # RISC-V 寄存器与宏定义
│   ├── sbi.h          # SBI 调用接口 <--- 新增
│   ├── string.h       # 字符串与内存操作
│   └── trap.h         # 中断处理接口 <--- 新增
├── kernel/
│   ├── boot/
│   │   └── entry.S    # 内核汇编入口
│   ├── driver/
│   │   └── sbi.c      # SBI 驱动实现 <--- 新增
│   ├── mm/
│   │   ├── kalloc.c   # 物理内存分配器
│   │   └── vm.c       # 虚拟内存（页表）管理
│   ├── trap/
│   │   ├── kernelvec.S # 中断汇编入口 <--- 新增
│   │   └── trap.c      # 中断 C 语言处理 <--- 新增
│   ├── console.c      # 控制台抽象层
│   ├── main.c         # 内核 C 主函数
│   ├── printf.c       # printf 实现
│   └── uart.c         # 串口 (UART) 驱动
├── scripts/
│   └── kernel.ld      # 链接器脚本
├── .gitignore
├── Makefile           # 自动化构建脚本
└── README.md          # 本说明文件
```
