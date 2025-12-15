#include "console.h"
#include "memory.h"
#include "trap.h"
#include "riscv.h"

// --- 实验四任务: 时钟中断测试 ---
void test_timer_interrupt(void) {
    printf("\n>>> Testing timer interrupt...\n");
    
    // 记录开始时的 tick 数
    uint64_t start_ticks = timer_ticks;
    uint64_t start_time = get_time();
    
    // 等待 5 次中断
    while (timer_ticks - start_ticks < 5) {
        // 忙等待，期间 CPU 会响应中断并增加 timer_ticks
        // 我们可以打印当前的等待状态，但不要打印太频繁以免刷屏
        if ((get_time() % 5000000) == 0) { // 稍微降低打印频率
             printf("Waiting... (current ticks: %d)\n", (int)timer_ticks);
        }
    }
    
    uint64_t end_time = get_time();
    printf("Timer test completed: 5 interrupts handled in %d cycles.\n", 
           (int)(end_time - start_time));
}

// --- 实验四任务: 性能测试 (简单测量) ---
void test_interrupt_overhead(void) {
    printf("\n>>> Testing interrupt overhead (latency)...\n");
    
    uint64_t start = get_time();
    
    // 我们做一个大量的空循环，看看中间是否被中断打断
    // 如果被中断打断，时间会显著增加
    for(volatile int i = 0; i < 10000000; i++);
    
    uint64_t end = get_time();
    
    printf("Loop completed. Duration: %d cycles.\n", (int)(end - start));
    printf("Check the timer ticks above to see if interrupts occurred during the loop.\n");
}

// --- 实验四任务: 异常处理测试 ---
void test_exception_handling(void) {
    printf("\n>>> Testing exception handling...\n");
    printf("Generating a Store Page Fault (writing to invalid address 0x0)...\n");
    printf("The kernel should catch this and print 'Unexpected trap' or panic.\n");
    
    // 故意写入非法地址 0x0 (内核未映射该地址，或该地址不可写)
    // 这将触发 scause = 15 (Store/AMO page fault)
    volatile int *bad_ptr = (int *)0x0;
    *bad_ptr = 42; 

    // 注意：由于当前的 kerneltrap 是死循环 (while(1))，程序执行到上面那行就会停止。
    // 所以这行代码永远不会执行到。
    printf("If you see this, exception handling failed!\n");
}

void main() {
    clear_screen();
    printf("====== RISC-V OS Booting ======\n");

    // 1. 初始化
    kinit();
    kvminit();
    kvminithart();
    trapinit();
    timerinit();
    
    // 2. 开启中断
    intr_on();
    printf_color(COLOR_YELLOW, "System initialized. Interrupts enabled.\n");

    // 3. 执行时钟中断测试
    test_timer_interrupt();

    // 4. 执行性能测试
    test_interrupt_overhead();

    // 5. 执行异常测试 (注意：这会导致系统停止/死循环)
    // 建议放在最后执行
    test_exception_handling();

    // 正常情况下，程序在 test_exception_handling 中就会因异常而停止
    // 如果注释掉上面的异常测试，程序会进入下面的死循环
    while (1) {
    }
}