#ifndef __SBI_H__
#define __SBI_H__

#include "riscv.h"

// SBI 调用号
#define SBI_SET_TIMER 0
#define SBI_CONSOLE_PUTCHAR 1
#define SBI_CONSOLE_GETCHAR 2

// SBI 接口函数
void sbi_set_timer(uint64_t stime);
void sbi_console_putchar(int ch);

#endif