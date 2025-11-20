#ifndef __TRAP_H__
#define __TRAP_H__

#include "riscv.h"

// 初始化中断系统
void trapinit(void);

// 初始化时钟
void timerinit(void);

// 中断处理函数
void kerneltrap(void);

// 时钟相关函数
void set_next_timer(void);
uint64_t get_time(void);
void cycles_to_time(uint64_t cycles, uint64_t *seconds, uint64_t *milliseconds);

// 新增以下两个函数声明
void usertrap(void);
void usertrapret(void);
#endif
