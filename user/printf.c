// user/printf.c
#include "types.h"
#include "stat.h"
#include "user/user.h"

#include <stdarg.h>

static char digits[] = "0123456789abcdef";

static void printint(int fd, int xx, int base, int sgn) {
  char buf[16];
  int i, neg;
  uint x;

  neg = 0;
  if(sgn && xx < 0){
    neg = 1;
    x = -xx;
  } else {
    x = xx;
  }

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);
  if(neg)
    buf[i++] = '-';

  while(--i >= 0)
    write(fd, &buf[i], 1);
}

void printf(const char *fmt, ...) {
  va_list ap;
  int i, c; // 将 c 改为 int 类型以匹配 va_arg
  char *s;

  va_start(ap, fmt);
  for(i = 0; (c = fmt[i] & 0xff) != 0; i++){
    if(c != '%'){
      write(1, &fmt[i], 1);
      continue;
    }
    c = fmt[++i] & 0xff;
    if(c == 0)
      break;
    switch(c){
    case 'd':
      printint(1, va_arg(ap, int), 10, 1);
      break;
    case 'x':
      printint(1, va_arg(ap, int), 16, 0);
      break;
    case 'p':
      printint(1, va_arg(ap, uint64), 16, 0);
      break;
    case 's':
      if((s = va_arg(ap, char*)) == 0)
        s = "(null)";
      for(; *s; s++)
        write(1, s, 1);
      break;
    case '%':
      write(1, "%", 1);
      break;
    default:
      // Print unknown % sequence to draw attention.
      write(1, "%", 1);
      write(1, &fmt[i], 1);
      break;
    }
  }
  va_end(ap);
}