# My RISC-V OS Project

## RISC-V OS - 实验五：进程管理与调度

这是一个从零开始构建的、逐步完善的 RISC-V 操作系统内核。本项目在实现了中断处理框架的基础上，迈出了质变的一步：构建抢占式多任务内核。

这使得我们的内核不再是一个只能被动响应中断的循环，而是一个可以主动管理、调度多个并发任务的真正意义上的“多任务操作系统”。

---

## 本项目目前已完成：

- **实验一：最小化内核启动**
  搭建了 QEMU 引导和串口输出的最小系统。

- **实验二：内核 printf 与清屏**
  实现了功能丰富的 printf 调试工具。

- **实验三：页表与内存管理**
  构建了物理内存分配器和 Sv39 三级页表虚拟内存系统。

- **实验四：中断处理与时钟管理**
  实现了 S 模式的中断处理框架和时钟中断响应。

- **实验五：进程管理与调度**
  实现了进程抽象、上下文切换和基于时钟中断的抢占式调度器。

---

## 功能实现

本项目在原有中断和内存管理的基础上，新增了完整的进程管理（proc）模块：

### 1. 进程抽象 (`include/proc.h`)

- 定义了进程的生命周期状态 `enum procstate`（如 UNUSED, RUNNABLE, RUNNING 等）。
- 定义了上下文切换所需的 `struct context`，用于保存 ra, sp 和 s0-s11 共 14 个被调用者保存寄存器。
- 定义了核心的进程控制块 `struct proc`，封装了进程状态、PID、内核栈 (kstack) 和上下文。

---

### 2. 上下文切换 (`kernel/proc/swtch.S`)

用汇编实现了 `swtch(struct context *old, struct context *new)` 函数。

- 保存 old 上下文的 14 个寄存器到内存
- 恢复 new 上下文的 14 个寄存器到 CPU
- 使用 `ret` 跳转到新进程 `ra` 并切换到新进程的 `sp`

---

### 3. 进程管理 (`kernel/proc/proc.c`)

- **procinit()**：初始化全局 `proc[NPROC]` 进程表
- **allocproc()**：创建新进程，分配 PID 和内核栈
- **scheduler()**：内核主循环，持续寻找 RUNNABLE 进程进行调度
- **yield()**：主动让出 CPU

---

### 4. 抢占式调度 (`kernel/trap/trap.c`)

- 在时钟中断中调用 `yield()`
- 即使进程死循环（比如 while(1)），也能被抢占
- 实现时间片轮转调度

---

### 5. 内核启动 (`kernel/main.c`)

- 初始化内存、设备、中断系统
- 调用 `procinit()` / `create_test_proc()`
- 最终进入 `scheduler()`，正式开始多任务调度

---

## 环境要求

- RISCV 交叉工具链
  `riscv64-unknown-elf-gcc`, `riscv64-unknown-elf-ld` …
- QEMU（riscv64）

---

## 如何构建和运行

本项目使用 Makefile 进行自动化管理。

### 编译内核

```bash
make
```
## 运行内核
```bash
 make qemu
 ```
 会看到多个进程的输出互相交织，说明抢占式调度生效。
 ## 调试内核
 ``` bash
 make debug
```
## 项目目录结构
```
.
├── include/
│ ├── console.h # 控制台与 printf 接口
│ ├── memory.h # 内存管理接口
│ ├── proc.h # 进程结构体定义 <--- 新增
│ ├── riscv.h # RISC-V 寄存器与宏定义
│ ├── sbi.h # SBI 调用接口
│ ├── string.h # 字符串与内存操作
│ └── trap.h # 中断处理接口
├── kernel/
│ ├── boot/
│ │ └── entry.S # 内核汇编入口
│ ├── driver/
│ │ └── sbi.c # SBI 驱动实现
│ ├── mm/
│ │ ├── kalloc.c # 物理内存分配器
│ │ └── vm.c # 虚拟内存（页表）管理
│ ├── proc/ # <--- 新增目录
│ │ ├── proc.c # 进程管理核心逻辑 <--- 新增
│ │ └── swtch.S # 上下文切换汇编 <--- 新增
│ ├── trap/
│ │ ├── kernelvec.S # 中断汇编入口
│ │ └── trap.c # 中断 C 语言处理
│ ├── console.c # 控制台抽象层
│ ├── main.c # 内核 C 主函数
│ ├── printf.c # printf 实现
│ └── uart.c # 串口 (UART) 驱动
├── scripts/
│ └── kernel.ld # 链接器脚本
├── .gitignore
├── Makefile # 自动化构建脚本
└── README.md # 本说明文件
```
这部分要实现的内容是目录操作