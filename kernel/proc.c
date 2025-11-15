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

// 辅助函数：初始化进程表
void procinit(void) {
    for(int i = 0; i < NPROC; i++) {
        proc[i].state = UNUSED;
    }
    printf_color(COLOR_YELLOW, "procinit: Process table initialized.\n");
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
    p->context.ra = (uint64_t)proc_entry_point;

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
        yield();
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
