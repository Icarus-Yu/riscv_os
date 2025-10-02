// include/memory.h
#ifndef __MEMORY_H__
#define __MEMORY_H__

#include "riscv.h" // 需要 riscv.h 中的类型定义

// --- kalloc.c ---
void kinit(void);
void* kalloc(void);
void kfree(void *pa);

// --- vm.c ---
void kvminit(void);
void kvminithart(void);

#endif // __MEMORY_H__
