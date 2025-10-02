// kernel/mm/vm.c

#include <console.h> // for printf
#include <memory.h>
#include "string.h"  // for memset

// 从虚拟地址 va 中提取第 level 级的页表索引
#define PX(level, va) ((((uint64_t) (va)) >> (PGSHIFT + 9 * (level))) & 0x1FF)

// 内核的根页表
pagetable_t kernel_pagetable;

// 查找 va 对应的PTE地址。如果中间页表不存在且 alloc=1, 则创建。
pte_t* walk(pagetable_t pagetable, uint64_t va, int alloc) {
    for(int level = 2; level > 0; level--) {
        pte_t *pte = &pagetable[PX(level, va)];
        if(*pte & PTE_V) {
            pagetable = (pagetable_t) ((((uint64_t)*pte) >> 10) << 12);
        } else {
            if(!alloc || (pagetable = (pagetable_t)kalloc()) == 0)
                return 0;
            // 将新分配的页清零
            memset(pagetable, 0, PGSIZE);
            // 设置PTE指向新的下一级页表
            *pte = ((((uint64_t)pagetable) >> 12) << 10) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}

// 创建一段虚拟地址到物理地址的映射
int mappages(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, int perm) {
    uint64_t a, last;
    pte_t *pte;

    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);
    for(;;){
        if((pte = walk(pagetable, a, 1)) == 0)
            return -1;
        if(*pte & PTE_V)
            // 如果PTE已经存在, 这是个错误
            return -1;
        *pte = ((((uint64_t)pa) >> 12) << 10) | perm | PTE_V;
        if(a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

// 创建内核页表
void kvminit(void) {
    // 分配一页作为根页表
    kernel_pagetable = (pagetable_t) kalloc();
    memset(kernel_pagetable, 0, PGSIZE);

    // 映射 UART 设备
    mappages(kernel_pagetable, 0x10000000L, PGSIZE, 0x10000000L, PTE_R | PTE_W);

    // 映射内核代码段 (R-X)
    extern char etext[];
    mappages(kernel_pagetable, 0x80000000L, (uint64_t)etext - 0x80000000L, 0x80000000L, PTE_R | PTE_X);

    // 映射内核数据和剩余物理内存 (R-W)
    mappages(kernel_pagetable, (uint64_t)etext, 0x88000000L - (uint64_t)etext, (uint64_t)etext, PTE_R | PTE_W);

    printf("kvminit: Kernel page table created.\n");
}

// 激活内核页表
void kvminithart(void) {
    uint64_t satp = (8L << 60) | (((uint64_t)kernel_pagetable) >> 12);
    asm volatile("csrw satp, %0" : : "r" (satp));
    asm volatile("sfence.vma zero, zero");
    printf("kvminithart: Paging enabled.\n");
}
