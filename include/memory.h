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

// 新增：暴露给 proc.c 使用的函数
int mappages(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, int perm);
pagetable_t uvmcreate(void); // 用于创建用户空页表
void uvmunmap(pagetable_t pagetable, uint64_t va, uint64_t npages, int do_free); // 用于释放
void freewalk(pagetable_t pagetable);
#endif // __MEMORY_H__
