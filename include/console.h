// include/console.h
#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <stdarg.h>

// --- ANSI Color Codes ---
#define COLOR_RESET   0
#define COLOR_RED     31
#define COLOR_GREEN   32
#define COLOR_YELLOW  33
#define COLOR_BLUE    34

// --- printf.c ---
void printf(const char *fmt, ...);
void clear_screen(void);
void goto_xy(int x, int y);
void printf_color(int color, const char *fmt, ...);

// --- console.c ---
void consputc(char c);

// --- uart.c ---
void uart_putc(char c);

#endif // __CONSOLE_H__
