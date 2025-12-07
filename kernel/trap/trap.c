#include "console.h"
#include "trap.h"
#include "sbi.h"
#include "proc.h"
#include "riscv.h"
#include "syscall.h"
extern void userret(uint64_t trapframe, uint64_t satp);
extern void uservec(void); // 声明外部汇编函数 用于陷阱处理
extern void kernelvec(void);
extern void syscall(void);
extern int sys_exit(void);
// 时钟中断间隔（0.1秒 = 1,000,000 cycles @ 10MHz）
#define TIMER_INTERVAL 1000000

// 中断计数器 - 必须初始化为 0
static volatile uint64_t timer_ticks = 0;

// 启动时间
static uint64_t boot_time = 0;

// 获取当前时间
uint64_t get_time(void) {
    return r_time();
}

// 将 cycles 转换为秒和毫秒
void cycles_to_time(uint64_t cycles, uint64_t *seconds, uint64_t *milliseconds) {
    *seconds = cycles / 10000000;
    *milliseconds = (cycles % 10000000) / 10000;
}

// 设置下一次时钟中断
void set_next_timer(void) {
    uint64_t next = r_time() + TIMER_INTERVAL;
    sbi_set_timer(next);
}

void trapinit(void) {
    extern void kernelvec();
    w_stvec((uint64_t)kernelvec);
    printf("trapinit: Trap vector initialized\n");
}

void timerinit(void) {
    // 确保 timer_ticks 为 0
    timer_ticks = 0;

    // 记录启动时间
    boot_time = get_time();

    // 开启时钟中断
    w_sie(r_sie() | SIE_STIE);

    // 设置第一次时钟中断
    set_next_timer();

    printf("timerinit: Timer initialized (interval=0.1s)\n");
    printf("timerinit: boot_time=%d cycles\n", (int)boot_time);
}

void kerneltrap(void) {
    uint64_t scause = r_scause();

    if (scause & (1ULL << 63)) {
        uint64_t interrupt_code = scause & 0x7FFFFFFFFFFFFFFF;

        if (interrupt_code == 5) {
            // 时钟中断
            timer_ticks = timer_ticks + 1;  // 明确的递增

            // 每10次中断打印一次
            if ((timer_ticks % 10) == 0) {
                uint64_t now = get_time();
                uint64_t uptime = now - boot_time;
                uint64_t seconds, milliseconds;

                cycles_to_time(uptime, &seconds, &milliseconds);

                printf_color(COLOR_GREEN,
                    "[Timer] Tick #%d | Cycles: %d\n",
                    (int)(timer_ticks & 0x7FFFFFFF),  // 只取低31位避免负数
                    (int)seconds,
                    (int)milliseconds,
                    (int)uptime);
            }
            // 设置下一次时钟中断
            set_next_timer();
            //新增实现抢占式调度
            //如果有进程正在运行，则强制让出cpu
            if(current_proc != 0 && current_proc->state == RUNNING) {
                yield();
            }

        } else {
            printf_color(COLOR_RED, "Unknown interrupt: %d\n", (int)interrupt_code);
        }
    } else {
        printf_color(COLOR_RED,
            "Unexpected trap: scause=0x%x, sepc=0x%x\n",
            (int)scause, (int)r_sepc());
        while(1);
    }
}
// 处理来自用户态的中断/异常
void usertrap(void) {
    struct proc *p = current_proc;
    uint64_t scause = r_scause();

    // 检查是否是系统调用 (ECALL from U-mode)
    // scause 为 8 代表 "Environment call from User mode"
    if (scause == 8) {
        // 1. 检查进程是否被杀
        if(p->state == ZOMBIE) // 简化检查
            sys_exit();

        // 2. EPC + 4
        // ecall 指令长度为 4 字节。如果不加 4，
        // 系统调用返回后会再次执行 ecall，导致死循环。
        p->trapframe->epc += 4;

        // 3. 开启中断
        // 系统调用通常是耗时的操作，允许在执行期间被时钟中断打断（抢占）
        intr_on();

        // 4. 执行系统调用
        syscall();
    }
    else if((scause & 0x8000000000000000L) && (scause & 0xff) == 5) {
        // 如果是用户态时的时钟中断
        set_next_timer();
        yield(); // 主动让出 CPU
    }
    else {
        printf("usertrap(): unexpected scause %p pid=%d\n", scause, p->pid);
        printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        sys_exit(); // 发生未知异常，杀死进程
    }

    // 准备返回用户态
    usertrapret();
}

// 返回用户态的准备工作
void usertrapret(void) {
    struct proc *p = current_proc;

    intr_off();

    // 设置 stvec 指向汇编入口 uservec
    uint64_t trampoline_uservec = (uint64_t)uservec;
    w_stvec(trampoline_uservec);

    // 设置 trapframe 中的内核信息，供下一次 uservec 使用
    p->trapframe->kernel_satp = r_satp();         // 内核页表
    p->trapframe->kernel_sp = p->kstack + PGSIZE; // 内核栈顶
    p->trapframe->kernel_trap = (uint64_t)usertrap; // C处理函数地址
    p->trapframe->kernel_hartid = r_tp();         // CPU核ID

    // 设置 SSTATUS
    // SPP = 0 (User mode), SPIE = 1 (Enable Interrupts)
    unsigned long x = r_sstatus();
    x &= ~SSTATUS_SPP;
    x |= SSTATUS_SPIE;
    w_sstatus(x);

    // 设置 SEPC (用户程序计数器)
    w_sepc(p->trapframe->epc);

    // --- 修改开始 ---
    // 构造用户页表的 satp 值
    uint64 satp = MAKE_SATP(p->pagetable);
    // --- 修改结束 ---
    
    // 调用汇编代码，跳回用户态！
    // 这是一个单向调用，不会返回
    uint64_t fn = (uint64_t)userret;
    ((void (*)(uint64_t, uint64_t))fn)((uint64_t)p->trapframe, satp);
}
