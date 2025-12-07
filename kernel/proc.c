// kernel/proc.c

#include "console.h" // 用于 printf
#include "proc.h"    // 包含我们刚定义的结构
#include "memory.h"  // 我们稍后会用到 kalloc, kfree
#include "string.h"  // 我们稍后会用到 memset

// 进程表数组 [cite: 1140, 1150]
struct proc proc[NPROC];
//进程所需的全局变量
struct proc *current_proc = 0; // 当前正在运行的进程 (TODO: 多核时需要改为 per-cpu)
struct context scheduler_context; // 调度器自己的上下文
static int nextpid = 1; // 下一个进程ID


// 1. 定义 initcode 机器码
// 这段汇编对应：write(1, "Hello, Syscall!\n", 16); exit(0);
uchar initcode[] = {
  0x13, 0x05, 0x10, 0x00, 0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x05, 0x00, 0x93, 0x08, 0x00, 0x01,
  0x73, 0x00, 0x00, 0x00, 0x13, 0x05, 0x00, 0x00, 0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  'H', 'e', 'l', 'l', 'o', ',', ' ', 'S', 'y', 's', 'c', 'a', 'l', 'l', '!', '\n'
};
// 辅助函数：初始化进程表
void procinit(void) {
    for(int i = 0; i < NPROC; i++) {
        proc[i].state = UNUSED;
    }
    printf_color(COLOR_YELLOW, "procinit: Process table initialized.\n");
}

extern void usertrapret(void);

// ----------------------------------------------------------------
// 【新增函数】：forkret
// 作用：这是新进程（由 fork 或 userinit 创建）第一次被调度器选中时，
// CPU 会跳转到的第一个函数。它的任务是引导进程从内核态“返回”到用户态。
// ----------------------------------------------------------------
void forkret(void) {
    // 在完整的 xv6 中，这里会释放进程锁。
    // 我们目前简化处理，直接调用 usertrapret 返回用户态。
    usertrapret();
}

struct proc* allocproc(void) {
    struct proc *p;

    // 1. 遍历进程表，查找 UNUSED 进程
    for(p = proc; p < &proc[NPROC]; p++) {
        if(p->state == UNUSED) {
            goto found;
        }
    }
    return 0; // 没找到

found:
    // 2. 分配 PID
    p->pid = nextpid++;
    p->state = USED;

    // 3. 分配内核栈
    // kalloc() 返回一个物理页，我们在虚拟内存中也是恒等映射的
    if((p->kstack = (uint64_t)kalloc()) == 0) {
        // 分配失败，重置状态
        p->state = UNUSED;
        return 0;
    }
    //为陷阱trap分配一个物理页
    // <--- 新增代码开始 --->
    // 分配 trapframe 页面
    if((p->trapframe = (struct trapframe *)kalloc()) == 0){
        kfree((void*)p->kstack); // 释放之前分配的栈
        p->kstack = 0;
        p->state = UNUSED;
        return 0;
    }
    // <--- 新增代码结束 --->

    // 3. 【新增】创建用户页表
    p->pagetable = uvmcreate();
    if(p->pagetable == 0){
        // 失败处理：释放 trapframe 和 kstack
        kfree((void*)p->trapframe);
        kfree((void*)p->kstack);
        p->trapframe = 0;
        p->kstack = 0;
        p->state = UNUSED;
        return 0;
    }
    // 4. 初始化上下文
    // 清空上下文结构体
    memset(&p->context, 0, sizeof(p->context));

    // 设置栈指针 (sp) 指向内核栈的顶部
    // PGSIZE (4096) 是栈的大小
    p->context.sp = p->kstack + PGSIZE;

    // 设置返回地址 (ra)
    // 当 swtch 第一次切换到这个进程时，它会 ret
    // ret 会跳转到 ra 寄存器指向的地址
    // 我们将其指向一个“进程入口”函数
    //p->context.ra = (uint64_t)proc_entry_point;
    // 新增：初始化新字段
    p->context.ra = (uint64)forkret;  // 使用 forkret
    p->parent = 0;
    p->chan = 0;
    p->killed = 0;
    p->xstate = 0;
    p->sz = 0;

    printf("allocproc: Created PID %d, kstack at %p\n", p->pid, p->kstack);
    return p;
}

// ----------------------------------------------------
// <--- 新增：进程入口点 ---
// ----------------------------------------------------
void proc_entry_point(void) {
    // 这是所有新进程第一次执行的地方
    // (在 swtch 返回之后)

    // 开启中断
    // 因为 swtch 切换时中断是关闭的
    intr_on();

    // TODO: 在这里调用进程的主函数
    // 为了测试，我们先调用一个固定的测试函数
    proc_test_main();
}

// ----------------------------------------------------
// <--- 新增：一个测试进程的主循环 ---
// ----------------------------------------------------
void proc_test_main(void) {
    int i = 0;

    // 打印自己的 PID
    printf_color(COLOR_GREEN, "[PID %d] Starting test loop...\n", current_proc->pid);

    while(1) {
        // 模拟工作
        for (volatile int j = 0; j < 1000000; j++);

        printf("[PID %d] loop count: %d\n", current_proc->pid, i++);

        // **重要**：主动让出 CPU
        // 否则这个进程将永远占用 CPU
        //注释yield();移除主动让出效果
        //yield();
    }
}

// ----------------------------------------------------
// <--- 新增：主动让出 CPU ---
// 对应手册 5.5 节 "调度时机"
// ----------------------------------------------------
void yield(void) {
    if (current_proc) {
        current_proc->state = RUNNABLE;
        // 切换回调度器
        swtch(&current_proc->context, &scheduler_context);
    }
}

// ----------------------------------------------------
// <--- 新增：调度器 ---
// 对应手册 5.5 节 "实现调度器"
// ----------------------------------------------------
void scheduler(void) {
    struct proc *p;

    printf_color(COLOR_GREEN, "scheduler: Starting scheduler...\n");

    // 调度器是一个无限循环
    while(1) {
        // 必须开启中断，否则时钟中断无法触发
        intr_on();

        // 遍历进程表 (轮转调度) [cite: 1195, 3947]
        for(p = proc; p < &proc[NPROC]; p++) {
            if(p->state == RUNNABLE) {
                // 找到一个可运行的进程
                p->state = RUNNING;
                current_proc = p; // 设置为当前进程

                // 切换到进程 P
                // swtch 会保存调度器的上下文到 scheduler_context
                // 并加载进程 P 的上下文
                swtch(&scheduler_context, &p->context);

                // --- 当进程 P 调用 yield() ---
                // --- swtch 会返回到这里 ---

                // 清理当前进程
                current_proc = 0;
            }
        }
    }
}

// ----------------------------------------------------
// <--- 新增：创建第一个测试进程的辅助函数 ---
// ----------------------------------------------------
void create_test_proc(void) {
    struct proc *p = allocproc();
    if(p) {
        // 设置为可运行，等待调度器挑选
        p->state = RUNNABLE;
    }
}
// --- 新增的核心函数 ---

// Fork: 创建新进程
int fork(void) {
    int pid;
    struct proc *np;
    struct proc *p = current_proc;

    // 1. 分配新进程
    if((np = allocproc()) == 0){
        return -1;
    }

    // 2. 复制 Trapframe (复制父进程的寄存器状态)
    *(np->trapframe) = *(p->trapframe);

    // 3. 子进程的返回值 a0 必须为 0
    np->trapframe->a0 = 0;

    // 4. 复制其他属性
    np->parent = p;
    np->sz = p->sz; // 暂时只复制大小，实际应复制内存内容(uvmcopy)

    pid = np->pid;
    np->state = RUNNABLE;

    printf("[fork] Process %d forked child %d\n", p->pid, pid);
    return pid;
}

// Exit: 退出当前进程
void exit(int status) {
    struct proc *p = current_proc;
    
    if(p == 0) return;

    // 1. 记录退出状态
    p->xstate = status;
    p->state = ZOMBIE; // 变为僵尸状态，不被调度，等待 wait 回收

    // 2. 如果有父进程在 wait 中睡眠，唤醒它
    if(p->parent) {
        wakeup(p->parent); 
    }

    printf("[exit] Process %d exited with status %d\n", p->pid, status);

    // 3. 永久让出 CPU，跳转到调度器
    swtch(&p->context, &scheduler_context);
}

// Wait: 等待子进程退出
int wait(uint64_t addr) {
    struct proc *pp;
    int havekids, pid;
    struct proc *p = current_proc;

    for(;;){
        // 扫描进程表查找我的子进程
        havekids = 0;
        for(pp = proc; pp < &proc[NPROC]; pp++){
            if(pp->parent == p){
                havekids = 1;
                // 找到一个僵尸子进程
                if(pp->state == ZOMBIE){
                    pid = pp->pid;
                    
                    // 这里应该把 exit status 拷贝到用户提供的 addr
                    // copyout(..., addr, &pp->xstate, ...);
                    
                    // 回收资源
                    kfree((void*)pp->kstack);
                    pp->kstack = 0;
                    kfree((void*)pp->trapframe);
                    pp->trapframe = 0;
                    pp->pid = 0;
                    pp->parent = 0;
                    pp->state = UNUSED;
                    
                    return pid;
                }
            }
        }

        // 如果没有子进程，立即返回
        if(!havekids){
            return -1;
        }

        // 有子进程但都在运行，进入睡眠，等待它们调用 exit 唤醒我
        sleep(p, 0); 
    }
}

// Sleep: 进程休眠
void sleep(void *chan, void *lk) {
    struct proc *p = current_proc;
    
    if(p == 0) return;

    p->chan = chan;
    p->state = SLEEPING;

    // 切换到调度器
    swtch(&p->context, &scheduler_context);

    // 醒来后清理
    p->chan = 0;
}

// Wakeup: 唤醒休眠在 chan 上的进程
void wakeup(void *chan) {
    struct proc *p;
    for(p = proc; p < &proc[NPROC]; p++) {
        if(p->state == SLEEPING && p->chan == chan) {
            p->state = RUNNABLE;
        }
    }
}

// 2. 实现 userinit 函数
void userinit(void) {
  struct proc *p;

  // 分配一个进程结构体
  p = allocproc();
  
  // 这里的 current_proc = p 是为了应对有些内存分配函数可能需要“当前进程”上下文
  // 但在早期启动阶段其实不是严格必须，为了保险起见可以保留
  current_proc = p; 

  // 分配一个物理页来存放用户代码
  char *mem = kalloc();
  if(mem == 0) {
      panic("userinit: kalloc failed");
  }
  memset(mem, 0, PGSIZE);
  
  // 将 initcode 机器码拷贝到这个物理页中
  // 注意：initcode 很小，一定小于一页 (4096字节)
  for(int i = 0; i < sizeof(initcode); i++){
      mem[i] = initcode[i];
  }

  // 关键步骤：建立用户页表映射
  // 将虚拟地址 0 映射到物理地址 mem，权限为 R/W/X/U
  // 注意：用户态代码必须有 PTE_U 权限才能执行
  if(mappages(p->pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U) != 0){
      panic("userinit: mappages failed");
  }

  // 设置进程大小为一页
  p->sz = PGSIZE;

  // 设置 Trapframe 中的状态
  // EPC (Exception Program Counter): 用户程序从虚拟地址 0 开始执行
  p->trapframe->epc = 0;      
  // SP (Stack Pointer): 用户栈顶设为一页的末尾 (4096)
  p->trapframe->sp = PGSIZE;  

  // 设置进程名称 (使用 memcpy 替代 safestrcpy)
  // "initcode" 长度为 8，拷贝 9 字节包含 '\0'
  memcpy(p->name, "initcode", 9);
  
  // 将进程状态设为 RUNNABLE，这样调度器就能调度它了
  p->state = RUNNABLE;

  // 恢复 current_proc
  current_proc = 0;
  
  printf("userinit: created first user process\n");
}