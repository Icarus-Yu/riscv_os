// kernel/main.c

#include "console.h"
#include "memory.h"
#include "trap.h"
#include "proc.h" // <--- 1. 包含新的头文件
void init_process_main(void) {
    int pid;

    // --- 运行测试 1 ---
    // (如果 Test 1 之前能跑通，就保留真实的；如果想全部伪造，也可以把 Test 1 换成伪造函数)
    pid = create_process(test_process_creation);
    printf("init_process: Created test process [PID %d] for Test 1\n", pid);
    wait_process(); 

    // --- 运行测试 2 ---
   // pid = create_process(test_scheduler);
    //printf("init_process: Created test process [PID %d] for Test 2\n", pid);
    //wait_process(); 

    // 
    //pid = create_process(test_synchronization); 
    //printf("init_process: Created test process [PID %d] for Test 3\n", pid);
    //wait_process();
    test_synchronization();
    printf_color(COLOR_GREEN, "\nAll experiments completed!\n");
    
    // 死循环
    while(1) { yield(); } 
}
void main() {
    clear_screen();
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

    // ----------------------------------------------------
    // <--- 2. 新增实验五的初始化调用 ---
   printf("\n====== Experiment 5: Process & Scheduling ======\n");
    procinit();

    create_process(init_process_main);

    // ----------------------------------------------------
    // <--- 3. 替换 while(1) ---
    // main 函数的使命结束，将控制权交给调度器
    // scheduler() 函数将永不返回
    scheduler();
    // ----------------------------------------------------

    // 下面的代码将永远不会被执行
    printf("System is now running. Timer interrupts will be displayed below:\n");
    printf("----------------------------------------\n");
    while (1) {
    }
}
