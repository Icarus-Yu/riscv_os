#include <console.h>

// 静态辅助函数声明
static void printint(long long xx, int base, int sign);
static void printptr(unsigned long long x);
static void vprintf(const char *fmt, va_list ap);

// printint - 打印一个带符号的整数 (迭代实现，避免栈溢出)
// xx: 要打印的数字
// base: 进制 (例如 10 或 16)
// sign: 是否处理符号 (1 表示处理, 0 表示不处理)
static void printint(long long xx, int base, int sign) {
    char buf[32];
    int i = 0;
    unsigned long long x;

    // 处理负数，并解决 INT_MIN 溢出问题
    if (sign && xx < 0) {
        consputc('-');
        x = -xx;
    } else {
        x = xx;
    }

    // 使用迭代法将数字转换为字符串，逆序存入 buf
    do {
        buf[i++] = "0123456789abcdef"[x % base];
    } while ((x /= base) != 0);

    // 将 buf 中的字符正序输出
    while (--i >= 0) {
        consputc(buf[i]);
    }
}

// printptr - 打印一个指针地址
// 地址以 0x 开头的十六进制格式输出
static void printptr(unsigned long long x) {
    consputc('0');
    consputc('x');
    // RISC-V 是 64 位架构, 所以指针是 8 字节, 16 个十六进制位
    for (int i = 0; i < (sizeof(unsigned long long) * 2); i++, x <<= 4) {
        consputc("0123456789abcdef"[x >> (sizeof(unsigned long long) * 8 - 4)]);
    }
}

// vprintf - printf 的核心实现，接收一个 va_list
// 这是 printf 和 printf_color 代码复用的关键
static void vprintf(const char *fmt, va_list ap) {
    char *s;
    int c;

    if (fmt == 0) {
        return;
    }

    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            consputc(*p);
            continue;
        }

        p++; // 跳过 '%'
        if (*p == '\0') {
             break; // 防止格式字符串以 '%' 结尾
        }

        switch (*p) {
            case 'd': // 整数
                printint(va_arg(ap, int), 10, 1);
                break;
            case 'x': // 十六进制
                printint(va_arg(ap, int), 16, 0);
                break;
            case 'p': // 指针
                printptr(va_arg(ap, unsigned long long));
                break;
            case 's': // 字符串
                if ((s = va_arg(ap, char *)) == 0) {
                    s = "(null)";
                }
                while (*s) {
                    consputc(*s++);
                }
                break;
            case 'c': // 字符
                c = va_arg(ap, int);
                consputc(c);
                break;
            case '%': // 百分号
                consputc('%');
                break;
            default: // 未知格式，直接打印
                consputc('%');
                consputc(*p);
                break;
        }
    }
}

// printf - 内核格式化输出主函数
void printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap); // 调用核心实现
    va_end(ap);
}

// printf_color - 以指定颜色打印格式化字符串
void printf_color(int color, const char *fmt, ...) {
    va_list ap;

    // 1. 设置颜色
    printf("\033[%dm", color);

    // 2. 调用 vprintf 核心实现
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    // 3. 恢复默认颜色
    printf("\033[%dm", COLOR_RESET);
}

// clear_screen - 清屏函数
void clear_screen(void) {
    // 发送 ANSI 转义序列: \033[2J 清除整个屏幕, \033[H 将光标移到左上角
    printf("\033[2J\033[H");
}

// goto_xy - 将光标移动到指定位置
// x: 列号 (column), y: 行号 (row)
void goto_xy(int x, int y) {
    // 发送 ANSI 转义序列 \033[<L>;<C>H
    printf("\033[%d;%dH", y, x);
}