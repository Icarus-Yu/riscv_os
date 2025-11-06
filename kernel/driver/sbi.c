#include "sbi.h"

// 通用 SBI 调用函数
static inline long sbi_call(long which, long arg0, long arg1, long arg2) {
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long a7 asm("a7") = which;
    
    asm volatile("ecall"
                 : "+r"(a0)
                 : "r"(a1), "r"(a2), "r"(a7)
                 : "memory");
    return a0;
}

// 设置下一次时钟中断
void sbi_set_timer(uint64_t stime) {
    sbi_call(SBI_SET_TIMER, stime, 0, 0);
}

// 通过 SBI 输出字符（备用）
void sbi_console_putchar(int ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}