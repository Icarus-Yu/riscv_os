// kernel/main.c

#include "console.h"
#include "memory.h"
#include "trap.h"
#include "proc.h" // <--- 1. 包含新的头文件
#include "buf.h"  // <--- 1. 新增：引入 buf.h 以修复 binit 报错

// <--- 2. 新增：手动声明磁盘驱动初始化函数
void virtio_disk_init(void);
void main() {
    clear_screen();
    trapinit();
    timerinit();
    intr_on();
    printf("====== RISC-V OS Booting ======\n");

    printf("\n====== Experiment 2: Printf Test ======\n");
    printf("Testing integer: %d\n", 12345);
    printf("Testing negative: %d\n", -54321);
    printf("Testing hex: 0x%x\n", 0xABCD);
    printf("Testing string: %s\n", "Hello, OS!");
    printf("Testing pointer: %p\n", (void*)0x80200000);

    printf("\n====== Experiment 3: Memory Management ======\n");
    kinit();
    kvminit();
    kvminithart();
    printf_color(COLOR_GREEN, "Virtual memory enabled!\n");

    printf("\n====== Experiment 4: Interrupt & Timer ======\n");
    trapinit();
    timerinit();

    intr_on();
    printf_color(COLOR_YELLOW, "Interrupts enabled! Waiting for timer...\n\n");
printf("\n====== Experiment 7: File System ======\n");
    
    // 初始化文件系统底层
    virtio_disk_init(); // 初始化磁盘驱动
    binit();            // 初始化缓冲区
    procinit();
    // ----------------------------------------------------
    // <--- 2. 新增实验五的初始化调用 ---
   printf("\n====== Experiment 5: Process & Scheduling ======\n");
    procinit();

    //create_test_proc(); // 创建第一个测试进程 (PID 1)
    //create_test_proc(); // 创建第二个测试进程 (PID 2)
    //// ----------------------------------------------------

    // ----------------------------------------------------
    // <--- 3. 替换 while(1) ---
    // main 函数的使命结束，将控制权交给调度器
    // scheduler() 函数将永不返回
    printf("\n====== Experiment 6: System Call Verification ======\n");
    // 创建第一个用户进程
    userinit(); 
    // --- 修改结束 ---
    scheduler();
    // ----------------------------------------------------
   
    
    // 下面的代码将永远不会被执行
    printf("System is now running. Timer interrupts will be displayed below:\n");
    printf("----------------------------------------\n");
    while (1) {
    }
}
