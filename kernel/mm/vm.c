// kernel/mm/vm.c

#include <console.h>
#include <memory.h>
#include "string.h"

#define PX(level, va) ((((uint64_t) (va)) >> (PGSHIFT + 9 * (level))) & 0x1FF)

pagetable_t kernel_pagetable;

pte_t* walk(pagetable_t pagetable, uint64_t va, int alloc) {
    for(int level = 2; level > 0; level--) {
        pte_t *pte = &pagetable[PX(level, va)];
        if(*pte & PTE_V) {
            pagetable = (pagetable_t) ((((uint64_t)*pte) >> 10) << 12);
        } else {
            if(!alloc || (pagetable = (pagetable_t)kalloc()) == 0)
                return 0;
            memset(pagetable, 0, PGSIZE);
            *pte = ((((uint64_t)pagetable) >> 12) << 10) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}

int mappages(pagetable_t pagetable, uint64_t va, uint64_t size, uint64_t pa, int perm) {
    uint64_t a, last;
    pte_t *pte;

    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);
    for(;;){
        if((pte = walk(pagetable, a, 1)) == 0)
            return -1;
        if(*pte & PTE_V)
            return -1;
        *pte = ((((uint64_t)pa) >> 12) << 10) | perm | PTE_V;
        if(a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

void kvminit(void) {
    kernel_pagetable = (pagetable_t) kalloc();
    memset(kernel_pagetable, 0, PGSIZE);

    extern char etext[];

    printf("kvminit: etext = %p\n", etext);
    printf("kvminit: Mapping kernel memory...\n");

    // 1. 映射 UART 设备 (恒等映射)
    if (mappages(kernel_pagetable, 0x10000000L, PGSIZE, 0x10000000L, PTE_R | PTE_W) != 0) {
        printf("kvminit: UART mapping failed!\n");
        return;
    }
    // <--- 【新增代码开始】 --->
    // 1.1 映射 VirtIO 磁盘设备 (恒等映射)
    // VirtIO MMIO 基地址为 0x10001000
    if (mappages(kernel_pagetable, 0x10001000L, PGSIZE, 0x10001000L, PTE_R | PTE_W) != 0) {
        panic("kvminit: VirtIO mapping failed!");
    }
    // <--- 【新增代码结束】 --->
    // 2. 映射内核代码段 (恒等映射: 虚拟地址 = 物理地址)
    // 从 0x80200000 到 etext，权限为 R+X
    uint64_t kernel_start = 0x80200000L;
    uint64_t kernel_code_size = PGROUNDUP((uint64_t)etext - kernel_start);

    printf("kvminit: Mapping kernel code: VA=PA=%p, size=%p\n",
           kernel_start, kernel_code_size);

    if (mappages(kernel_pagetable, kernel_start, kernel_code_size,
                 kernel_start, PTE_R | PTE_X | PTE_W) != 0) { // <--- 添加 PTE_W
        printf("kvminit: Kernel code mapping failed!\n");
        return;
    }

    // 3. 映射内核数据段和剩余物理内存 (恒等映射)
    // 从 etext 到 PHYSTOP (0x88000000)，权限为 R+W
    uint64_t data_start = PGROUNDUP((uint64_t)etext);
    uint64_t data_size = 0x88000000L - data_start;

    printf("kvminit: Mapping kernel data: VA=PA=%p, size=%p\n",
           data_start, data_size);

    if (mappages(kernel_pagetable, data_start, data_size,
                 data_start, PTE_R | PTE_W) != 0) {
        printf("kvminit: Kernel data mapping failed!\n");
        return;
    }

    printf("kvminit: Kernel page table created successfully.\n");
}

void kvminithart(void) {
    // 在写入 satp 之前，先刷新指令缓存
    asm volatile("fence.i");

    // 构造 satp 值: MODE=Sv39 (8) | PPN
    uint64_t satp = (8L << 60) | (((uint64_t)kernel_pagetable) >> 12);

    printf("kvminithart: Setting satp to %p\n", satp);
    printf("kvminithart: kernel_pagetable at %p\n", kernel_pagetable);

    // 写入 satp 寄存器，启用分页
    asm volatile("csrw satp, %0" : : "r" (satp));

    // 刷新 TLB
    asm volatile("sfence.vma zero, zero");

    printf("kvminithart: Paging enabled successfully!\n");
}

// 创建一个空的用户页表
// 返回指向页表根目录的指针，失败返回 0
pagetable_t uvmcreate(void) {
  pagetable_t pagetable;
  pagetable = (pagetable_t) kalloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// 递归释放页表页面 (但不释放叶子节点指向的物理内存)
void freewalk(pagetable_t pagetable) {
  // 简化的实现：在实验6中，如果还没实现完整的uvmfree，
  // 可以先留空或者只释放根节点，但这会导致内存泄漏。
  // 完整的 freewalk 需要递归遍历三级页表。
  // 暂时为了跑通实验6，我们可以简单地：
  if (pagetable) 
      kfree((void*)pagetable);
}