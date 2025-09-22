#include "console.h"
#include <stdarg.h>
//对于数字打印的处理，设计为静态函数
static void print_number(long long num,int base,int sign){
    char buf[64];
    const char *digits = "0123456789abcdef";

    if (sign && num < 0) {
        uart_putc('-');
        num = -num;
    }

    int i = 0;
    do {
        buf[i++] = digits[num % base];
        num /= base;
    } while (num > 0);

    while (--i >= 0) {
        uart_putc(buf[i]); 
 }
}

// printf 的主体实现
void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            uart_putc(*p);
            continue;
        }

        p++; // 跳过 '%'

        switch (*p) {
            case 'd': // 十进制
                print_number(va_arg(ap, int), 10, 1);
                break;
            case 'x': // 十六进制
                print_number(va_arg(ap, int), 16, 0);
                break;
            case 's': { // 字符串
                char *s = va_arg(ap, char *);
                if (!s) {
                    s = "(null)";
                }
                while(*s) {
                    uart_putc(*s++);
                }
                break;
            }
            case 'c': // 字符
                uart_putc(va_arg(ap, int));
                break;
            case '%': // 百分号
                uart_putc('%');
                break;
            default: // 未知格式
                uart_putc('%');
                uart_putc(*p);
                break;
        }
    }

    va_end(ap);
}
// 清屏函数实现
void clear_screen(void) {
    // 发送ANSI转义序列: \033 是 ESC 的八进制表示
    // [2J 表示清除整个屏幕
    uart_puts("\033[2J"); 
    // [H 表示将光标移动到左上角
    uart_puts("\033[H");
}