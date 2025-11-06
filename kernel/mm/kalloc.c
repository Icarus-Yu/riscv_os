// kernel/mm/kalloc.c

#include <console.h>
#include <memory.h>
#include "string.h"

extern char etext[];

// 物理内存结束地址
#define PHYSTOP 0x88000000L

struct run {
    struct run *next;
};

struct {
    struct run *freelist;
} kmem;

void kfree(void *pa) {
    struct run *r;
    r = (struct run*)pa;
    r->next = kmem.freelist;
    kmem.freelist = r;
}

void kinit() {
    // 从 etext（现在是 0x80200000 之后）到 PHYSTOP
    for (char *p = (char*)PGROUNDUP((uint64_t)etext); 
         p + PGSIZE <= (char*)PHYSTOP; 
         p += PGSIZE) {
        kfree(p);
    }
    printf("kinit: Physical memory allocator initialized.\n");
}

void* kalloc(void) {
    struct run *r;
    r = kmem.freelist;
    if (r) {
        kmem.freelist = r->next;
    }
    return (void*)r;
}