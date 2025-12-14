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
//内核查找虚拟地址对应的物理地址 (exec需要它来检查内存)
uint64 walkaddr(pagetable_t pagetable, uint64 va);
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len);
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max);

// 新增：暴露给 proc.c 使用的函数
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
pagetable_t uvmcreate(void); // 用于创建用户空页表
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free); // 用于释放
void freewalk(pagetable_t pagetable);

// 【新增】
uint64 walkaddr(pagetable_t pagetable, uint64 va);
void uvmclear(pagetable_t pagetable, uint64 va);
int copyin(pagetable_t, char *, uint64, uint64);
int copyinstr(pagetable_t, char *, uint64, uint64);

#endif // __MEMORY_H__
