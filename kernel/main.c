#include "console.h"
#include "memory.h"

// 声明在 uart.c 中定义的函数
void uart_puts(char *s);

// C代码入口函数，函数名需与 entry.S 中的 `call` 指令对应
void main() {
    int test_var = 42;
    int *ptr = &test_var;
    clear_screen();
    printf("====== Experiment 2 Test Start ======\n");
    printf("Testing integer: %d\n", 12345);
    printf("Testing negative integer: %d\n", -54321);
    printf("Testing zero: %d\n", 0);
    printf("Testing hex: 0x%x\n", 0xABCD);
    printf("Testing string: %s\n", "Hello, layered OS!");
    printf("Testing char: %c\n", 'Z');
    printf("Testing percent: %%\n");

    // 测试指针打印
    printf("Testing pointer address: %p\n", ptr);
    printf("Testing NULL pointer: %p\n", (void *)0);

    printf("====== Experiment 2 Test End ======\n");
     printf("====== Color Test Start ======\n");

    // 使用我们新定义的宏
    printf_color(COLOR_RED, "This text is RED!\n");
    printf_color(COLOR_GREEN, "This text is GREEN! And the number is %d\n", 123);
    printf_color(COLOR_YELLOW, "This text is YELLOW! Pointer: %p\n", (void*)0x80200000);
    printf_color(COLOR_BLUE, "This text is BLUE!\n");

    printf("This text should be back to the default color.\n");
    printf("====== Color Test End ======\n");
    // for (volatile int i = 0; i < 20000000; i++) {
    //     // This is a clear, intentionally empty loop body.
    // }

    printf("====== OS Starting ======\n");

    // 初始化物理内存分配器
    kinit();

    // 创建内核页表
    kvminit();

    // 激活分页
    kvminithart();

    printf("====== Paging Activated, Hello Virtual Memory! ======\n");
    // 任务完成后，进入死循环



    while (1);
}
