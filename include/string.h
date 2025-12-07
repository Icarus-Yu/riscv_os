// include/string.h  <-- (修正注释)
#ifndef __STRING_H__  
#define __STRING_H__  
#include "riscv.h" // 需要 riscv.h 中的类型定义

// --- kalloc.c ---
void kinit(void);
void* kalloc(void);
void kfree(void *pa);


// --- vm.c ---
void kvminit(void);
void kvminithart(void);
void* memset(void *dst, int c, uint64_t n);
// 新增 memcpy 声明
void* memcpy(void *dst, const void *src, uint64_t n);

#endif // __MEMORY_H__
