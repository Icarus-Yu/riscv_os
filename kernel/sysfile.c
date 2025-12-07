#include "riscv.h"
#include "proc.h"
#include "syscall.h"
#include "console.h"
#include "memory.h"
//文件处理相关
extern pte_t* walk(pagetable_t pagetable, uint64 va, int alloc);
// 引用 syscall.c 中的 argint
extern int argint(int n, int *ip);
extern int argaddr(int n, uint64_t *ip);
extern int consgetc(void);
// 写文件系统调用
// 参数: fd (a0), buf (a1), count (a2)
// 写文件系统调用
int sys_write(void) {
    int fd;
    uint64_t p; 
    int n;

    // 获取参数
    if (argint(0, &fd) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
        return -1;
    
    // 目前只支持向 stdout (1) 和 stderr (2) 写入
    if (fd == 1 || fd == 2) {
        for (int i = 0; i < n; i++) {
            // 1. 计算当前字符的用户虚拟地址
            uint64 va = p + i;
            
            // 2. 查用户的页表，找到对应的页表项 (PTE)
            pte_t *pte = walk(current_proc->pagetable, va, 0);
            
            // 3. 检查地址是否有效
            if(pte == 0 || (*pte & PTE_V) == 0) {
                return -1; // 地址未映射
            }
            
            // 4. 获取物理地址
            uint64 pa = PTE2PA(*pte);      // 获取该页的物理基址
            uint64 offset = va & 0xFFF;    // 获取页内偏移
            char *ch_addr = (char *)(pa + offset); // 组合成完整的物理地址
            
            // 5. 在内核中，物理地址是直接映射的，可以直接读取
            consputc(*ch_addr);
        }
        return n;
    }
    return -1;
}

// 新增：sys_read
int sys_read(void) {
    int fd;
    int n;
    uint64_t p;

    if (argint(0, &fd) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
        return -1;

    // 目前只支持从标准输入 (fd=0) 读取
    if (fd == 0) {
        char *buf = (char *)p;
        for(int i = 0; i < n; i++) {
            int c = consgetc(); // 读取字符
            buf[i] = c;
            consputc(c); // 回显字符
            if(c == '\n' || c == '\r') {
                return i + 1; // 遇到换行符提前返回
            }
        }
        return n;
    }
    return -1;
}