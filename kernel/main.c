// kernel/main.c

#include "console.h"
#include "memory.h"
#include "trap.h"
#include "proc.h"
#include "buf.h" 

// 手动声明未在头文件中暴露的初始化函数
void virtio_disk_init(void);

void main() {
    // 1. 基础 UI 初始化
    clear_screen();
    printf("====== RISC-V OS Booting ======\n");

    // 2. 内存管理初始化 (物理内存 -> 内核页表 -> 开启分页)
    printf("[Boot] Initializing Memory...\n");
    kinit();         // 物理内存分配器
    kvminit();       // 创建内核页表
    kvminithart();   // 开启分页机制 (写入 satp)
    printf_color(COLOR_GREEN, " - Memory initialized.\n");

    // 3. 中断与时钟初始化
    printf("[Boot] Initializing Interrupts...\n");
    trapinit();      // 设置中断向量
    timerinit();     // 设置时钟中断
    printf_color(COLOR_GREEN, " - Interrupts initialized.\n");

    // 4. 文件系统与设备初始化 (实验 7 新增)
    printf("[Boot] Initializing File System...\n");
    virtio_disk_init(); // 初始化磁盘驱动
    binit();            // 初始化缓冲区缓存
    // iinit();         // (可选) 如果你实现了 inode 缓存初始化，可以在这里调用
    printf_color(COLOR_GREEN, " - File System initialized.\n");

    // 5. 进程管理初始化
    printf("[Boot] Initializing Process Manager...\n");
    procinit();      // 初始化进程表
    userinit();      // 创建第一个用户进程 (initcode)
    printf_color(COLOR_GREEN, " - First user process created.\n");

    // 6. 开启中断并启动调度器
    printf("[Boot] System Ready. Handing over to scheduler...\n");
    printf("---------------------------------------------\n");
    
    intr_on();       // 开启全局中断
    scheduler();     // 进入调度循环 (永不返回)

    // 7. 死循环 (防御性编程，理论上永远不会执行到这里)
    panic("main: scheduler returned");
}