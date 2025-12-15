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

// 进程控制块 (PCB)
// 对应手册 5.1 节 "进程结构体定义"
struct proc {
    enum procstate state; // 进程状态
    int pid;              // 进程 ID
    uint64_t kstack;      // 内核栈的基地址 (虚拟地址)
    struct context context; // 进程的上下文
    void (*fn)(void);
    struct proc *parent; // 新增：记录父进程
    // 我们将在后续步骤中添加更多字段，例如：
    // pagetable_t pagetable; // 用户页表
    // struct trapframe *trapframe; // 陷阱帧
    // struct proc *parent; // 父进程
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
// [新增] 修改/新增以下函数原型
int create_process(void (*entry)(void)); // 通用进程创建
void exit_process(void);                 // 进程退出
int wait_process(void);                  // 等待并回收僵尸进程

// [新增] 测试函数
void test_process_creation(void);
void test_scheduler(void);
void test_synchronization(void);
#endif // __PROC_H__
