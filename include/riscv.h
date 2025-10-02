// include/riscv.h
#ifndef __RISCV_H__
#define __RISCV_H__

#include <stdint.h> // 用于 uint64_t 等类型

// 页面大小为 4096 字节
#define PGSIZE 4096
// 页面偏移量的位数
#define PGSHIFT 12

// 将地址向下对齐到页面边界
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))
// 将地址向上对齐到页面边界
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))

// 页表项 (PTE) 的权限位
#define PTE_V (1L << 0) // Valid
#define PTE_R (1L << 1) // Read
#define PTE_W (1L << 2) // Write
#define PTE_X (1L << 3) // Execute
#define PTE_U (1L << 4) // User

// 页表项类型定义
typedef uint64_t pte_t;
// 页表类型定义 (一个指向PTE数组的指针)
typedef uint64_t *pagetable_t;

#endif // __RISCV_H__
