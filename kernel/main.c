#include "console.h"

// 声明在 uart.c 中定义的函数
void uart_puts(char *s);

// C代码入口函数，函数名需与 entry.S 中的 `call` 指令对应
void main() {
    //清屏操作
    clear_screen();
   printf("====== printf test start ======\n");
    printf("Testing integer: %d\n", 42);
    printf("Testing negative: %d\n", -123);
    printf("Testing zero: %d\n", 0);
    printf("Testing hex: 0x%x\n", 0xABC);
    printf("Testing string: %s\n", "Hello RISC-V OS!");
    printf("Testing char: %c\n", 'A');
    printf("Testing percent: %%\n");
    printf("====== printf test end ======\n");
    clear_screen();
    // 任务完成后，进入死循环，避免CPU失控
    while (1);
}
