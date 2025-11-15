#include "console.h"
#include "trap.h"
#include "sbi.h"
#include "proc.h"

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
