

# My RISC-V OS Project

## RISC-V OS - 实验六：系统调用 (System Calls)

这是一个从零开始构建的、逐步完善的 RISC-V 操作系统内核。本项目在实现了多任务调度的基础上，进一步打通了**用户态（User Mode）与内核态（Kernel Mode）**的边界，构建了完整的系统调用框架，使得操作系统能够服务于用户程序。

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

- **实验六：系统调用** <-- **NEW**
  实现了 `fork`, `exit`, `wait`, `write` 等核心系统调用，成功加载并运行了第一个用户态进程 (`initcode`)。

---

## 功能实现

### 1. 进程抽象 (`include/proc.h`)
- 定义了进程状态 (`UNUSED`, `RUNNABLE`, `RUNNING`, `ZOMBIE` 等)。
- 定义了 `struct context` 保存寄存器上下文。
- 定义了 `struct proc`，包含内核栈、用户页表、Trapframe（陷阱帧）等关键字段。

### 2. 上下文切换 (`kernel/proc/swtch.S`)
- 实现了 `swtch` 函数，在进程内核线程与调度器线程之间切换寄存器状态。

### 3. 抢占式调度 (`kernel/proc/proc.c`)
- **scheduler()**：内核主循环，采用轮转调度算法 (Round Robin)。
- **yield()**：在时钟中断 (`timer_interrupt`) 中主动让出 CPU，实现抢占。

### 4. 用户态支持与系统调用 (实验六新增)

在此阶段，我们实现了从内核态向用户态的跨越：

- **第一个用户进程 (`kernel/proc.c: userinit`)**
  - 手动将一段二进制机器码 (`initcode`) 拷贝到物理内存。
  - 建立用户页表映射，配置 Trapframe，使系统启动后能自动进入用户态执行。

- **Trap 分发与处理 (`kernel/trap/trap.c`)**
  - **usertrap()**：识别来自用户态的异常。当 `scause` 为 8 时，识别为系统调用 (`ecall`)，将 PC+4 并分发给 syscall 处理。
  - **Trampoline (`kernel/trap/trampoline.S`)**：实现了 `uservec` 和 `userret`，负责用户态与内核态之间寄存器的保存与恢复，以及页表的切换。

- **系统调用框架 (`kernel/syscall.c`)**
  - **syscall()**：统一分发入口，通过 `a7` 寄存器获取调用号，从 `syscalls[]` 表中调用对应内核函数，并将返回值写入 `a0`。
  - **参数获取**：实现了 `argint`，从当前进程的 Trapframe 中读取用户传递的参数。

- **已实现的核心调用**：
  - **进程控制**：
    - `sys_fork`：复制当前进程（包括内存和状态），实现进程克隆。
    - `sys_exit`：进程退出，释放资源并唤醒父进程，状态转为 ZOMBIE。
    - `sys_wait`：父进程回收僵尸子进程资源。
    - `sys_getpid`：获取当前进程 ID。
  - **文件/控制台 I/O**：
    - `sys_write` / `sys_read`：通过查询页表将用户虚拟地址转换为物理地址，实现了面向控制台的基础输入输出。

---

## 环境要求

- RISCV 交叉工具链 (`riscv64-unknown-elf-gcc` 等)
- QEMU (`qemu-system-riscv64`)
- Make

---

## 如何构建和运行

### 编译内核
```bash
make
运行内核
Bash

make qemu
预期输出： 系统启动后将初始化各个模块，创建第一个用户进程。你将看到：

userinit: created first user process
scheduler: Starting scheduler...
Hello, Syscall!
[exit] Process 1 exited with status 0
这表明用户程序成功执行了 write 系统调用打印字符串，并调用 exit 正常退出。

调试内核
Bash

make debug
可以使用 GDB 连接 localhost:1234 进行断点调试。

项目目录结构
.
├── include/        # 头文件 (syscall.h, proc.h, trap.h 等)
├── kernel/
│   ├── boot/       # 启动汇编
│   ├── driver/     # 硬件驱动 (UART, SBI)
│   ├── mm/         # 内存管理 (kalloc, vm)
│   ├── proc/       # 进程管理 (proc.c, swtch.S)
│   ├── trap/       # 中断与异常 (trap.c, trampoline.S, kernelvec.S)
│   ├── syscall.c   # 系统调用分发 <-- Ex6 核心
│   ├── sysproc.c   # 进程类系统调用实现
│   ├── sysfile.c   # 文件类系统调用实现
│   ├── main.c      # 内核入口
│   └── ...
├── scripts/        # 链接脚本
├── Makefile        # 构建脚本
└── README.md       # 项目说明