// kernel/proc.c

#include "console.h" // 用于 printf
#include "proc.h"    // 包含我们刚定义的结构
#include "memory.h"  // 我们稍后会用到 kalloc, kfree
#include "string.h"  // 我们稍后会用到 memset

// 进程表数组 [cite: 1140, 1150]
struct proc proc[NPROC];

// 辅助函数：初始化进程表
void procinit(void) {
    for(int i = 0; i < NPROC; i++) {
        proc[i].state = UNUSED;
    }
    printf_color(COLOR_YELLOW, "procinit: Process table initialized.\n");
}

