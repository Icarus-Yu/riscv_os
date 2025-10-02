// kernel/mm/kalloc.c

#include <console.h> // for printf
#include <memory.h>
#include "string.h"  // for memset

extern char etext[]; // 内核代码段结束地址, 由链接脚本提供

// 物理内存的起始和结束地址
#define PHYSTOP 0x88000000L

// 空闲页链表的节点结构
struct run {
    struct run *next;
};

// 管理空闲内存的结构体
struct {
    struct run *freelist;
} kmem;

// 将一个物理页回收到空闲链表
void kfree(void *pa) {
    struct run *r;

    // 简单地将页面地址转换为 run 结构体指针
    r = (struct run*)pa;
    // 将其插入到链表头部
    r->next = kmem.freelist;
    kmem.freelist = r;
}

// 初始化物理内存分配器
void kinit() {
    // 将从 etext 到 PHYSTOP 的所有内存逐页释放
    for (char *p = (char*)PGROUNDUP((uint64_t)etext); p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE) {
        kfree(p);
    }
    printf("kinit: Physical memory allocator initialized.\n");
}

// 分配一个 4KB 的物理页
void* kalloc(void) {
    struct run *r;

    // 从空闲链表头部取出一个节点
    r = kmem.freelist;
    if (r) {
        kmem.freelist = r->next;
    }
    return (void*)r;
}
