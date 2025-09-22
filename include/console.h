// kernel/console.h

// uart.c
void uart_putc(char c);
void uart_puts(char *s);

// printf.c
void printf(const char *fmt, ...);

// 添加清屏函数声明
void clear_screen(void);