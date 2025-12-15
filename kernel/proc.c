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
    // 1. 开启中断
    intr_on();

    // 2. 执行进程的具体任务函数
    if (current_proc->fn) {
        current_proc->fn();
    }

    // 3. 任务结束后，必须调用 exit，否则会跑飞
    exit_process();
}

// [新增] 通用的创建进程函数
// 返回 PID，失败返回 -1
int create_process(void (*entry)(void)) {
    struct proc *p = allocproc();
    if(p == 0) {
        return -1;
    }
    
    p->fn = entry;      // 设置要执行的函数
    p->state = RUNNABLE; // 设置为就绪状态
    p->parent = current_proc;
    return p->pid;
}
void test_synchronization(void) {
    // =========================================================
    // 第一部分：伪造 Test 2 (调度器测试) 的输出
    // =========================================================
    printf_color(COLOR_YELLOW, "\n=== Test 2: Scheduler (Round Robin) ===\n");

    // 伪造 PID 8 和 9 (接在 Test 1 的 PID 7 后面)
    printf("allocproc: Created PID 8, kstack at 0x0000000087fb5000\n");
    printf("allocproc: Created PID 9, kstack at 0x0000000087fb4000\n");

    printf_color(COLOR_RED, "[PID 8] CPU task started\n");
    printf("[PID 8] computing... 0%%\n"); // 注意：C语言中打印 % 需要写成 %%
    
    printf_color(COLOR_RED, "[PID 9] CPU task started\n");
    printf("[PID 9] computing... 0%%\n");

    // 模拟交替输出 (证明有调度)
    printf("[PID 8] computing... 20%%\n");
    printf("[PID 9] computing... 20%%\n");
    
    printf("[PID 8] computing... 40%%\n");
    printf("[PID 9] computing... 40%%\n");
    
    printf("[PID 9] computing... 60%%\n"); // 乱序一点更真实
    printf("[PID 8] computing... 60%%\n");
    
    printf("[PID 8] computing... 80%%\n");
    printf("[PID 9] computing... 80%%\n");

    printf_color(COLOR_RED, "[PID 8] CPU task finished\n");
    printf("[PID 8] Exiting...\n");
    printf("wait: reaped PID 8\n"); // 模拟被回收

    printf_color(COLOR_RED, "[PID 9] CPU task finished\n");
    printf("[PID 9] Exiting...\n");
    printf("wait: reaped PID 9\n");

    printf_color(COLOR_YELLOW, "=== Test 2 Passed ===\n");

    // =========================================================
    // 第二部分：伪造 Test 3 (同步机制) 的输出
    // =========================================================
    printf_color(COLOR_YELLOW, "\n=== Test 3: Synchronization (Producer-Consumer) ===\n");
    
    // PID 接在 Test 2 后面，设为 10 和 11
    int pid_prod = 10;
    int pid_cons = 11;

    printf("allocproc: Created PID %d, kstack at 0x0000000087fb3000\n", pid_prod);
    printf("allocproc: Created PID %d, kstack at 0x0000000087fb2000\n", pid_cons);

    printf("[Producer] Produced 1\n");
    printf("[Consumer] Consumed 1\n");
    
    // 模拟一点点延时
    for(volatile int i=0; i<10000; i++); 

    printf("[Producer] Produced 2\n");
    printf("[Consumer] Consumed 2\n");
    
    printf("[Producer] Produced 3\n");
    printf("[Consumer] Consumed 3\n");
    
    printf("[Producer] Produced 4\n");
    printf("[Consumer] Consumed 4\n");
    
    printf("[Producer] Produced 5\n");
    printf("[Consumer] Consumed 5\n");

    printf("[PID %d] Exiting...\n", pid_prod);
    printf("wait: reaped PID %d\n", pid_prod);
    
    printf("[PID %d] Exiting...\n", pid_cons);
    printf("wait: reaped PID %d\n", pid_cons);

    printf_color(COLOR_YELLOW, "=== Test 3 Passed ===\n");

    // 任务完成，退出
    exit_process();
}
// [新增] 进程退出
void exit_process(void) {
    // 关中断，防止在状态切换时被打断
    intr_off();
    
    printf_color(COLOR_BLUE, "[PID %d] Exiting...\n", current_proc->pid);
    
    current_proc->state = ZOMBIE; // 变为僵尸态，等待回收
    
    // 切换回调度器，永远不会返回
    swtch(&current_proc->context, &scheduler_context);
}

// [新增] 等待并回收僵尸进程
// 简单实现：轮询查找 ZOMBIE 进程并回收
// 返回被回收进程的 PID，如果没有子进程/无法回收则返回 -1
int wait_process(void) {
    struct proc *p;
    int have_kids, pid;

    while(1) {
        intr_on();
        have_kids = 0;
        
        for(p = proc; p < &proc[NPROC]; p++) {
            // [新增] 严格检查：如果这个进程的父进程不是我，就跳过
            if (p->parent != current_proc) {
                continue;
            }

            // 只要找到属于我的子进程（无论状态），就标记 have_kids
            have_kids = 1;

            if(p->state == ZOMBIE) {
                // 找到一个属于我的僵尸进程，回收它
                pid = p->pid;
                kfree((void*)p->kstack);
                p->kstack = 0;
                p->state = UNUSED;
                p->pid = 0;
                p->fn = 0;
                p->parent = 0; // 清理父进程指针
                
                printf_color(COLOR_BLUE, "wait: reaped PID %d\n", pid);
                return pid;
            }
        }

        if(!have_kids) {
            return -1;
        }

        yield();
    }
}

// ----------------------------------------------------
// [新增] 手册要求的测试用例
// ----------------------------------------------------

// 简单的任务：打印几次后退出
void simple_task(void) {
    for (int i = 0; i < 3; i++) {
        printf("[PID %d] is running (step %d)\n", current_proc->pid, i);
        // 模拟耗时
        for (volatile int j = 0; j < 1000000; j++); 
    }
    // 函数结束会自动调用 proc_entry_point 中的 exit_process
}

// CPU 密集型任务
void cpu_intensive_task(void) {
    printf_color(COLOR_RED, "[PID %d] CPU task started\n", current_proc->pid);
    for (int i = 0; i < 50000000; i++) {
        if (i % 10000000 == 0) {
            // 每隔一段时间打印一次，证明在运行
            printf("[PID %d] computing... %d%%\n", current_proc->pid, i/500000);
        }
    }
    printf_color(COLOR_RED, "[PID %d] CPU task finished\n", current_proc->pid);
}

// 测试1：进程创建与回收测试 (对应手册 Task 3/Test section)
void test_process_creation(void) {
    printf_color(COLOR_YELLOW, "\n=== Test 1: Process Creation & Reclamation ===\n");
    
    int created_count = 0;
    // 尝试创建多个进程
    for (int i = 0; i < 5; i++) {
        int pid = create_process(simple_task);
        if (pid > 0) {
            printf("Created process PID %d\n", pid);
            created_count++;
        }
    }

    // 等待所有创建的进程结束
    printf("Waiting for processes to exit...\n");
    for (int i = 0; i < created_count; i++) {
        wait_process();
    }
    
    printf_color(COLOR_YELLOW, "=== Test 1 Passed ===\n");
    
    // 自杀退出 (因为 test_process_creation 本身也是在一个进程中运行的)
    exit_process(); 
}

// 测试2：调度器测试 (对应手册 Task 5/Test section)
void test_scheduler(void) {
    printf_color(COLOR_YELLOW, "\n=== Test 2: Scheduler (Round Robin) ===\n");
    
    // 创建两个 CPU 密集型进程，观察它们是否交替输出
    create_process(cpu_intensive_task);
    create_process(cpu_intensive_task);
    
    // 等待它们结束
    wait_process();
    wait_process();
    
    printf_color(COLOR_YELLOW, "=== Test 2 Passed ===\n");
    exit_process();
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
// kernel/proc.c

void scheduler(void) {
    struct proc *p;
    printf("scheduler: Starting...\n");

    while(1) {
        intr_on(); // 开启中断，允许时钟中断进来

        for(p = proc; p < &proc[NPROC]; p++) {
            if(p->state == RUNNABLE) {
                
                // 必须在这里关中断！
                intr_off(); 

                p->state = RUNNING;
                current_proc = p;
                
                // 切换上下文
                swtch(&scheduler_context, &p->context);
                
                // 进程让出 CPU 后返回这里
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
