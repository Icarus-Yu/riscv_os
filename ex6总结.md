这是一个非常扎实的实验六总结报告，专门针对验收场景进行了梳理。这份总结剔除了中间的试错过程（如 Ex5 残留代码导致的死循环、未映射跳板页导致的崩溃等），只保留了最终生效的正确路径和关键技术点。

实验六：系统调用 (System Calls) - 实验总结报告
1. 实验概述
本实验在已有的内核基础（内存管理、中断处理、内核线程调度）之上，打通了**用户态（User Mode）与内核态（Kernel Mode）**的交互链路。实现了进程的完整生命周期管理、系统调用分发机制，并成功构建并运行了第一个用户态进程 initcode，验证了 fork, write, exit 等核心系统调用的功能。

2. 核心功能实现 (Key Implementations)
A. 进程管理体系完善
扩展进程控制块 (struct proc):

在 include/proc.h 中新增了关键字段：parent（父进程）、pagetable（用户页表）、trapframe（陷阱帧）、xstate（退出状态）、sz（内存大小）等。

新增进程状态 ZOMBIE 和 SLEEPING。

实现核心生命周期函数 (kernel/proc.c):

fork(): 实现了进程的克隆。包括分配新 proc 结构、复制 trapframe（保留寄存器状态）、设置父子关系。

exit(): 实现了进程退出逻辑。标记状态为 ZOMBIE，记录退出码，唤醒父进程，并永久让出 CPU。

wait(): 实现了父进程回收子进程逻辑。扫描进程表查找僵尸子进程，释放其内核栈和 trapframe，避免僵尸进程泄漏。

allocproc() 升级: 增加了用户页表初始化 (uvmcreate) 和 Trapframe 分配。最关键的是将新进程的上下文返回地址 ra 修改为 forkret，确保新进程被调度时能正确返回用户态。

B. 系统调用分发机制
Trap 分发 (kernel/trap/trap.c):

在 usertrap() 中识别 scause == 8 (Environment Call)，将 epc 加 4（跳过 ecall 指令），并调用 syscall()。

参数获取与执行 (kernel/syscall.c):

实现了 syscall() 函数，通过 p->trapframe->a7 获取系统调用号，查表调用对应内核函数，并将返回值写入 p->trapframe->a0。

实现了 argint 等辅助函数，从 Trapframe 中提取用户传递的参数。

C. 具体的系统调用实现
文件/控制台操作 (kernel/sysfile.c):

sys_write: 实现了向控制台输出字符的功能。

sys_read: 实现了从控制台读取字符的功能（依赖 uart_getc）。

进程操作 (kernel/sysproc.c):

实现了 sys_fork, sys_exit, sys_wait, sys_getpid 等接口的内核侧封装。

D. 第一个用户进程 (userinit)
编写了 userinit() 函数，手动将一段二进制机器码 (initcode) 拷贝到物理内存。

手动建立用户页表映射（虚拟地址 0 -> 物理地址），并设置 Trapframe 的 epc=0 和 sp=PGSIZE。

这是验证系统调用链路的起点。

3. 攻克的关键技术难题 (Key Challenges Solved)
在开发过程中，我们遇到并解决了以下几个涉及操作系统核心机制的棘手问题：

问题 1：内核无法直接访问用户指针 (sys_write 无输出)
现象：系统调用成功触发，但打印不出字符或打印乱码。

原因：用户传入的 buffer 指针是用户态虚拟地址。在内核态（S-Mode）下，页表已切换为内核页表，直接解引用该地址会导致访问错误物理内存。

解决方案：在 sys_write 中引入页表查询机制。通过 walk() 函数查询当前进程的 pagetable，将用户虚拟地址翻译为物理地址，然后再进行读取。

问题 2：用户态页表切换崩溃 (Trampoline/Userret Crash)
现象：scheduler 启动后无任何输出，系统挂起。

原因：当 userret 汇编代码执行 csrw satp, ... 切换到用户页表后，CPU 立即无法读取下一条指令（因为用户页表中没有映射内核代码）。

解决方案：

代码层面：取消了 trampoline.S 中切换页表指令 (csrw satp) 的注释。

映射层面：在 allocproc 中，将内核的所有物理内存（代码段+数据段）映射到了用户页表中（权限 R/W/X）。这确保了切换页表后，CPU 依然能访问到内核的指令和数据，从而顺利执行后续的 sret 进入用户态。

问题 3：新进程无法进入用户态 (Loop in Kernel)
现象：新进程被调度后，错误地进入了 Ex5 的内核测试循环。

原因：allocproc 中上下文的返回地址 ra 默认指向了内核测试入口。

解决方案：引入 forkret 函数。将新进程的 context.ra 设置为 forkret，而 forkret 直接调用 usertrapret，从而引导新进程正确地通过 userret 返回到用户空间。

问题 4：缺少基础库函数
现象：编译报错 undefined reference to memcpy/memset/panic。

解决方案：手动实现了 memcpy (用于 struct 复制)、panic (用于错误停机) 等基础工具函数，并补充了 uchar, uint64 等类型定义。

4. 最终验证结果 (Final Verification)
执行 make qemu 后，系统成功启动并输出了预期的结果，证明全链路打通：

Plaintext

====== Experiment 6: System Call Verification ======
userinit: created first user process    <-- 1. 用户进程创建成功
scheduler: Starting scheduler...        <-- 2. 调度器启动
Hello, Syscall!                         <-- 3. 用户态 initcode 执行 write 系统调用成功
[exit] Process 1 exited with status 0   <-- 4. 用户态调用 exit 成功，内核回收资源
结论：实验六所有目标均已达成，系统具备了运行用户态程序和处理系统调用的能力，可以进行下一阶段（文件系统）的开发。