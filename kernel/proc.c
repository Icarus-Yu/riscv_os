// kernel/proc.c

#include "console.h"
#include "proc.h"
#include "memory.h"
#include "string.h"
#include "riscv.h"
#include "spinlock.h"

struct proc proc[NPROC];
struct proc *current_proc = 0;
struct context scheduler_context;
static int nextpid = 1;
extern char etext[]; 

extern void forkret(void);
extern void usertrapret(void);
// 显式声明 mappages (如果 memory.h 包含有问题，这里保底)
extern int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);

// 1. initcode 机器码 (Hello, Syscall!)
uchar initcode[] = {
    0x13, 0x05, 0x10, 0x00, 0x93, 0x05, 0x00, 0x02, 
    0x13, 0x06, 0x00, 0x01, 0x93, 0x08, 0x00, 0x01, 
    0x73, 0x00, 0x00, 0x00, 0x13, 0x05, 0x00, 0x00, 
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    'H', 'e', 'l', 'l', 'o', ',', ' ', 'S', 'y', 's', 'c', 'a', 'l', 'l', '!', '\n'
};

void procinit(void) {
    struct proc *p;
    for(p = proc; p < &proc[NPROC]; p++) {
        p->state = UNUSED;
    }
    printf_color(COLOR_YELLOW, "procinit: Process table initialized.\n");
}

// 【新增】辅助函数：为进程创建页表，并映射内核空间
pagetable_t proc_pagetable(struct proc *p) {
    pagetable_t pagetable;

    // 1. 创建空页表
    pagetable = uvmcreate();
    if(pagetable == 0) return 0;

    // 2. 映射内核代码和数据 (128MB: 0x80000000 ~ 0x88000000)
    // 权限: R | W | X (让内核在 S-mode 下可以读写执行)
    if(mappages(pagetable, 0x80000000L, 0x8000000L, 
                0x80000000L, PTE_R | PTE_W | PTE_X) != 0){
        goto fail;
    }

    // 3. 【关键】映射 UART 设备 (否则 printf 会崩)
    if(mappages(pagetable, 0x10000000L, PGSIZE, 
                0x10000000L, PTE_R | PTE_W) != 0){
        goto fail;
    }

    // 4. 【关键】映射 VirtIO 设备 (否则磁盘读写会崩)
    if(mappages(pagetable, 0x10001000L, PGSIZE, 
                0x10001000L, PTE_R | PTE_W) != 0){
        goto fail;
    }

    return pagetable;

fail:
    // 如果映射失败，释放页表 (简化处理，只释放页表页，不释放映射的物理页)
    // freewalk(pagetable); // 需要 memory.h 声明 freewalk
    return 0;
}

// 释放进程页表
void proc_freepagetable(pagetable_t pagetable, uint64 sz) {
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1); // 解除用户空间映射并释放物理页
    freewalk(pagetable); // 释放页表本身
}

struct proc* allocproc(void) {
    struct proc *p;

    for(p = proc; p < &proc[NPROC]; p++) {
        if(p->state == UNUSED) {
            goto found;
        }
    }
    return 0;

found:
    p->pid = nextpid++;
    p->state = USED;

    // 分配内核栈
    if((p->kstack = (uint64)kalloc()) == 0) {
        p->state = UNUSED;
        return 0;
    }

    // 分配 Trapframe
    if((p->trapframe = (struct trapframe *)kalloc()) == 0){
        kfree((void*)p->kstack);
        p->state = UNUSED;
        return 0;
    }

    // 创建页表 (包含内核映射)
    p->pagetable = proc_pagetable(p);
    if(p->pagetable == 0){
        printf("allocproc: failed to map kernel/devices\n"); // 调试信息
        kfree((void*)p->trapframe);
        kfree((void*)p->kstack);
        p->state = UNUSED;
        return 0;
    }

    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64)forkret; // 返回到 forkret -> usertrapret
    p->context.sp = p->kstack + PGSIZE;

    return p;
}

void userinit(void) {
    struct proc *p;

    p = allocproc();
    
    // 【修复】检查 p 是否为空
    if(p == 0) panic("userinit: allocproc failed");

    // 分配用户内存并拷贝 initcode
    char *mem = kalloc();
    if(mem == 0) panic("userinit: kalloc failed");
    memset(mem, 0, PGSIZE);
    
    // 这里的 memcpy 需要 string.h
    // 如果没有 memcpy，可以用循环
    char *src = (char*)initcode;
    for(int i = 0; i < sizeof(initcode); i++) mem[i] = src[i];

    // 映射到用户空间 (虚拟地址 0)
    if(mappages(p->pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U) != 0){
        panic("userinit: mappages failed");
    }

    p->sz = PGSIZE;
    
    // 设置 Trapframe，准备返回用户态
    p->trapframe->epc = 0;      // 用户程序从 0 开始执行
    p->trapframe->sp = PGSIZE;  // 用户栈顶 (暂用同一页的末尾)

    // 设置当前目录
    p->cwd = namei("/"); 

    // 设置进程名
    // memcpy(p->name, "initcode", 9); 
    // 简单赋值
    char *name = "initcode";
    for(int i=0; i<9; i++) p->name[i] = name[i];

    p->state = RUNNABLE;
    
    printf("userinit: created first user process\n");
}

// ... 以下函数 (fork, exit, wait, sleep, wakeup, yield, scheduler, etc.) 保持不变 ...
// 请确保你保留了原本文件中的 fork, exit, wait 等函数的实现代码
// 这里为了篇幅省略，请务必保留！

// (以下是占位，请保留你原本的实现)
void forkret(void) { usertrapret(); }
int fork(void) { /* ... 你的实现 ... */ return 0; }
void exit(int status) { /* ... 你的实现 ... */ }
int wait(uint64 addr) { /* ... 你的实现 ... */ return -1; }
void sleep(void *chan, void *lk) { /* ... */ }
void wakeup(void *chan) { /* ... */ }
void yield(void) { 
    if(current_proc) { 
        current_proc->state = RUNNABLE; 
        swtch(&current_proc->context, &scheduler_context); 
    } 
}
void scheduler(void) {
    struct proc *p;
    for(;;){
        intr_on();
        for(p = proc; p < &proc[NPROC]; p++) {
            if(p->state == RUNNABLE) {
                p->state = RUNNING;
                current_proc = p;
                swtch(&scheduler_context, &p->context);
                current_proc = 0;
            }
        }
    }
}