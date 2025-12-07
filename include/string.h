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

void* memmove(void *dst, const void *src, uint64 n); // <--- 新增
int strncmp(const char *p, const char *q, uint64 n); // <--- 新增
int strlen(const char *s);                           // <--- 新增(备用)
char* strncpy(char *s, const char *t, int n);        // <--- 新增(备用)
char* safestrcpy(char *s, const char *t, int n);     // <--- 新增(备用)
#endif // __MEMORY_H__
