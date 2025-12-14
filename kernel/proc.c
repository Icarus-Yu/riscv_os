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

// 【修复1】添加 namei 的外部声明
extern struct inode* namei(char* path);

// 【修复2】删除 mappages 的手动声明，memory.h 中已有
// extern int mappages(...) 

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

// 辅助函数：为进程创建页表，并映射内核空间
pagetable_t proc_pagetable(struct proc *p) {
    pagetable_t pagetable;

    // 1. 创建空页表
    pagetable = uvmcreate();
    if(pagetable == 0) return 0;

    // 2. 映射内核代码和数据 (128MB: 0x80000000 ~ 0x88000000)
    if(mappages(pagetable, 0x80000000L, 0x8000000L, 
                0x80000000L, PTE_R | PTE_W | PTE_X) != 0){
        goto fail;
    }

    // 3. 映射 UART 设备
    if(mappages(pagetable, 0x10000000L, PGSIZE, 
                0x10000000L, PTE_R | PTE_W) != 0){
        goto fail;
    }

    // 4. 映射 VirtIO 设备
    if(mappages(pagetable, 0x10001000L, PGSIZE, 
                0x10001000L, PTE_R | PTE_W) != 0){
        goto fail;
    }

    return pagetable;

fail:
    // freewalk(pagetable); 
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
        printf("allocproc: failed to map kernel/devices\n");
        kfree((void*)p->trapframe);
        kfree((void*)p->kstack);
        p->state = UNUSED;
        return 0;
    }

    memset(&p->context, 0, sizeof(p->context));
    p->context.ra = (uint64)forkret;
    p->context.sp = p->kstack + PGSIZE;

    return p;
}

void userinit(void) {
    struct proc *p;

    p = allocproc();
    if(p == 0) panic("userinit: allocproc failed");

    // 分配用户内存并拷贝 initcode
    char *mem = kalloc();
    if(mem == 0) panic("userinit: kalloc failed");
    memset(mem, 0, PGSIZE);
    
    char *src = (char*)initcode;
    for(int i = 0; i < sizeof(initcode); i++) mem[i] = src[i];

    // 映射到用户空间
    if(mappages(p->pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U) != 0){
        panic("userinit: mappages failed");
    }

    p->sz = PGSIZE;
    p->trapframe->epc = 0;
    p->trapframe->sp = PGSIZE;

    // 设置当前目录
    p->cwd = namei("/"); 

    char *name = "initcode";
    for(int i=0; i<9; i++) p->name[i] = name[i];

    p->state = RUNNABLE;
    
    printf("userinit: created first user process\n");
}

// ... fork, exit, wait 等 ...

void forkret(void) {
    usertrapret();
}

int fork(void) {
    int pid;
    struct proc *np;
    struct proc *p = current_proc;

    if((np = allocproc()) == 0){
        return -1;
    }

    *(np->trapframe) = *(p->trapframe);
    np->trapframe->a0 = 0;
    np->parent = p;
    np->sz = p->sz; // 注意：这里需要深拷贝内存(uvmcopy)，当前仅为简化演示

    pid = np->pid;
    np->state = RUNNABLE;

    printf("[fork] Process %d forked child %d\n", p->pid, pid);
    return pid;
}

void exit(int status) {
    struct proc *p = current_proc;
    if(p == 0) return;

    p->xstate = status;
    p->state = ZOMBIE;

    if(p->parent) {
        wakeup(p->parent); 
    }

    printf("[exit] Process %d exited with status %d\n", p->pid, status);
    swtch(&p->context, &scheduler_context);
}

// 【修复3】类型改为 uint64_t
int wait(uint64_t addr) {
    struct proc *pp;
    int havekids, pid;
    struct proc *p = current_proc;

    for(;;){
        havekids = 0;
        for(pp = proc; pp < &proc[NPROC]; pp++){
            if(pp->parent == p){
                havekids = 1;
                if(pp->state == ZOMBIE){
                    pid = pp->pid;
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

        if(!havekids){
            return -1;
        }

        sleep(p, 0); 
    }
}

void sleep(void *chan, void *lk) {
    struct proc *p = current_proc;
    if(p == 0) return;

    p->chan = chan;
    p->state = SLEEPING;
    swtch(&p->context, &scheduler_context);
    p->chan = 0;
}

void wakeup(void *chan) {
    struct proc *p;
    for(p = proc; p < &proc[NPROC]; p++) {
        if(p->state == SLEEPING && p->chan == chan) {
            p->state = RUNNABLE;
        }
    }
}

void yield(void) { 
    if(current_proc) { 
        current_proc->state = RUNNABLE; 
        swtch(&current_proc->context, &scheduler_context); 
    } 
}

void scheduler(void) {
    struct proc *p;
    printf_color(COLOR_GREEN, "scheduler: Starting scheduler...\n");
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