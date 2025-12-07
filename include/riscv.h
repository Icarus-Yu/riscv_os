#ifndef __RISCV_H__
#define __RISCV_H__

#include <stdint.h>
#include "types.h"
// // --- 新增：常用类型缩写 ---
// typedef unsigned char uchar;
// typedef unsigned int  uint;
// typedef unsigned short ushort;
// typedef uint64_t uint64; // 
// typedef uint32_t uint32; // <--- 新增
// typedef uint16_t uint16; // <--- 新增
// // -----------------------
// 页面大小
#define PGSIZE 4096
#define PGSHIFT 12

#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))

// 页表项权限位
#define PTE_V (1L << 0)
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4)

typedef uint64_t pte_t;
typedef uint64_t *pagetable_t;

#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_SPP (1L << 8)  // Supervisor Previous Privilege
// === 新增：中断相关的 CSR 寄存器操作 ===

// 读取 sstatus 寄存器
static inline uint64_t r_sstatus() {
    uint64_t x;
    asm volatile("csrr %0, sstatus" : "=r" (x));
    return x;
}

// 写入 sstatus 寄存器
static inline void w_sstatus(uint64_t x) {
    asm volatile("csrw sstatus, %0" : : "r" (x));
}

// sstatus 寄存器的位定义
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable

// 读取 sie 寄存器（中断使能）
static inline uint64_t r_sie() {
    uint64_t x;
    asm volatile("csrr %0, sie" : "=r" (x));
    return x;
}

// 写入 sie 寄存器
static inline void w_sie(uint64_t x) {
    asm volatile("csrw sie, %0" : : "r" (x));
}

// sie 寄存器的位定义
#define SIE_SEIE (1L << 9)  // 外部中断
#define SIE_STIE (1L << 5)  // 时钟中断
#define SIE_SSIE (1L << 1)  // 软件中断
#define MAKE_SATP(pagetable) (8L << 60 | (uint64)pagetable >> 12)
// SATP 构造宏 (Sv39模式: Mode=8)
// 读取 scause 寄存器（中断/异常原因）
static inline uint64_t r_scause() {
    uint64_t x;
    asm volatile("csrr %0, scause" : "=r" (x));
    return x;
}

// 读取 sepc 寄存器（异常返回地址）
static inline uint64_t r_sepc() {
    uint64_t x;
    asm volatile("csrr %0, sepc" : "=r" (x));
    return x;
}

// 写入 sepc 寄存器
static inline void w_sepc(uint64_t x) {
    asm volatile("csrw sepc, %0" : : "r" (x));
}

// 读取 stvec 寄存器（中断向量地址）
static inline uint64_t r_stvec() {
    uint64_t x;
    asm volatile("csrr %0, stvec" : "=r" (x));
    return x;
}

// 写入 stvec 寄存器
static inline void w_stvec(uint64_t x) {
    asm volatile("csrw stvec, %0" : : "r" (x));
}

// 读取 time 寄存器（当前时间）
static inline uint64_t r_time() {
    uint64_t x;
    asm volatile("csrr %0, time" : "=r" (x));
    return x;
}

// 开启中断
static inline void intr_on() {
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// 关闭中断
static inline void intr_off() {
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// 检查中断是否开启
static inline int intr_get() {
    uint64_t x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

// 读取 stval 寄存器 (Scause Trap Value)
static inline uint64_t r_stval() {
    uint64_t x;
    asm volatile("csrr %0, stval" : "=r" (x));
    return x;
}

// 读取 satp 寄存器 (Supervisor Address Translation and Protection)
static inline uint64_t r_satp() {
    uint64_t x;
    asm volatile("csrr %0, satp" : "=r" (x));
    return x;
}

// 读取 tp 寄存器 (Thread Pointer, 用于保存 hartid)
static inline uint64_t r_tp() {
    uint64_t x;
    asm volatile("mv %0, tp" : "=r" (x));
    return x;
}

// include/riscv.h
#define PTE2PA(pte) (((pte) >> 10) << 12)
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
#endif
