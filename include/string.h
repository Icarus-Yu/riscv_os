// include/string.h  <-- (修正注释)
#ifndef __STRING_H__  // <-- 修改为独一无二的宏
#define __STRING_H__  // <-- 保持一致
#include "riscv.h" // 需要 riscv.h 中的类型定义

// --- kalloc.c ---
void kinit(void);
void* kalloc(void);
void kfree(void *pa);


// --- vm.c ---
void kvminit(void);
void kvminithart(void);
void* memset(void *dst, int c, uint64_t n);

#endif // __MEMORY_H__
