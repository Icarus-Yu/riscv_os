// include/proc.h
#ifndef __PROC_H__
#define __PROC_H__

#include "riscv.h" // 需要 riscv.h 中的类型定义，例如 uint64_t

// 进程状态
enum procstate {
    UNUSED,   // 未使用
    USED,     // 已分配，但尚未完全初始化
    SLEEPING, // 睡眠中（等待某个事件）
    RUNNABLE, // 可运行（在就绪队列中，等待被调度）
    RUNNING,  // 运行中（正在占用 CPU）
    ZOMBIE    // 僵尸（已退出，等待父进程回收）
};

// 进程的上下文 (Context)
// swtch.S 会使用这个结构来保存和恢复寄存器
// 它只保存被调用者（callee-saved）寄存器
struct context {
    uint64_t ra; // 返回地址
    uint64_t sp; // 栈指针

    // 被调用者保存的寄存器
    uint64_t s0;
    uint64_t s1;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
};


// 最大进程数
#define NPROC 64

// 陷阱帧：用于保存用户态中断/异常时的寄存器 [cite: 1100-1102, 1469]
struct trapframe {
  /* 0 */ uint64_t kernel_satp;   // kernel page table
  /* 8 */ uint64_t kernel_sp;     // top of process's kernel stack
  /* 16 */ uint64_t kernel_trap;   // usertrap()
  /* 24 */ uint64_t epc;           // saved user program counter
  /* 32 */ uint64_t kernel_hartid; // saved kernel tp
  /* 40 */ uint64_t ra;
  /* 48 */ uint64_t sp;
  /* 56 */ uint64_t gp;
  /* 64 */ uint64_t tp;
  /* 72 */ uint64_t t0;
  /* 80 */ uint64_t t1;
  /* 88 */ uint64_t t2;
  /* 96 */ uint64_t s0;
  /* 104 */ uint64_t s1;
  /* 112 */ uint64_t a0;
  /* 120 */ uint64_t a1;
  /* 128 */ uint64_t a2;
  /* 136 */ uint64_t a3;
  /* 144 */ uint64_t a4;
  /* 152 */ uint64_t a5;
  /* 160 */ uint64_t a6;
  /* 168 */ uint64_t a7;
  /* 176 */ uint64_t s2;
  /* 184 */ uint64_t s3;
  /* 192 */ uint64_t s4;
  /* 200 */ uint64_t s5;
  /* 208 */ uint64_t s6;
  /* 216 */ uint64_t s7;
  /* 224 */ uint64_t s8;
  /* 232 */ uint64_t s9;
  /* 240 */ uint64_t s10;
  /* 248 */ uint64_t s11;
  /* 256 */ uint64_t t3;
  /* 264 */ uint64_t t4;
  /* 272 */ uint64_t t5;
  /* 280 */ uint64_t t6;
};
// 进程控制块 (PCB)
// 对应手册 5.1 节 "进程结构体定义"
// 修改 struct proc，启用 trapframe 字段
struct proc {
    enum procstate state;   // 进程状态
    int pid;                // 进程 ID
    uint64_t kstack;        // 内核栈的基地址 (虚拟地址)
    struct context context; // 进程的上下文

    // 启用 trapframe
    struct trapframe *trapframe; // 陷阱帧
    
    // pagetable_t pagetable; // 用户页表 (稍后启用)
    // struct proc *parent; // 父进程 (稍后启用)
};

// --- proc.c 中的函数原型 ---

// 初始化进程表
void procinit(void);

// 上下文切换函数 (在 swtch.S 中实现)
void swtch(struct context *old, struct context *new);


struct proc* allocproc(void); // 分配进程
void scheduler(void);     // 调度器
void yield(void);         // 主动让出
void create_test_proc(void); // 创建测试进程
void proc_entry_point(void); // 进程入口点 (内部使用)
void proc_test_main(void);   // 测试进程主函数 (内部使用)

// 外部变量
extern struct proc *current_proc; // 当前运行的进程

#endif // __PROC_H__
