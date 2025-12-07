#include <console.h>
// consputc - 控制台字符输出函数
// 这是 printf 和底层硬件驱动之间的中间人。
// 目前，它只是简单地调用 uart_putc。
// 未来，这里可以添加缓冲区、锁，并决定将字符发送到多个设备（如屏幕）。
void consputc(char c) {
    uart_putc(c);
}

extern int uart_getc(void);

// 新增：控制台读取函数 (阻塞式)
// 在本次实验中，我们用简单的轮询来实现阻塞读取
int consgetc(void) {
    int c;
    while(1) {
        c = uart_getc();
        if(c != -1) {
            return c;
        }
        // 在实际 OS 中，这里应该让进程 sleep 等待中断
        // 但为了简化实验 6，我们暂时使用忙等待
    }
}