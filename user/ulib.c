// user/ulib.c
#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user/user.h"

char* strcpy(char *s, const char *t) {
  char *os;
  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int strcmp(const char *p, const char *q) {
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

uint strlen(const char *s) {
  int n;
  for(n = 0; s[n]; n++)
    ;
  return n;
}

void* memset(void *dst, int c, uint n) {
  char *cdst = (char *) dst;
  int i;
  for(i = 0; i < n; i++){
    cdst[i] = c;
  }
  return dst;
}

char* strchr(const char *s, char c) {
  for(; *s; s++)
    if(*s == c)
      return (char*)s;
  return 0;
}

void* malloc(uint n) {
  // 暂时简化，仅仅是一个占位符，init 暂时用不到 malloc
  // 完整实现需要 sbrk
  return 0;
}

void free(void *ap) {
}