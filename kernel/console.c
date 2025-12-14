// kernel/console.c
#include "types.h"
#include "riscv.h"
#include "spinlock.h"
#include "console.h"
#include "file.h" // for devsw
#include "proc.h" // for copyin/copyout

// 声明外部的 file.h 中的设备开关表
extern struct devsw devsw[];

// 简单的控制台写函数：直接把数据塞给 UART
int consolewrite(int user_src, uint64 src, int n) {
  int i;
  char c;

  for(i = 0; i < n; i++){
    // 从源地址（可能是用户空间）读取一个字符
    // 为了简化，目前 kernel/printf 用的是内核地址，init 用的是用户地址
    // 这里我们先假设是用户地址，使用 copyin
    // 但注意：kernel printf 也会调用 consputc，那是另一条路
    
    // 如果是 sys_write 调用的，src 是用户虚拟地址
    // 我们一个字节一个字节地拷贝
    if(copyin(current_proc->pagetable, &c, src + i, 1) == -1)
      break;
      
    uart_putc(c);
  }
  return i;
}

// 简单的控制台读函数
int consoleread(int user_dst, uint64 dst, int n) {
  // 暂时留空，这一步只为了看到输出
  // 下一步做 Shell 输入时再完善这里
  return 0;
}

// 初始化控制台驱动
void consoleinit(void) {
  // 注册控制台设备 (Major = 1)
  devsw[CONSOLE].read = consoleread;
  devsw[CONSOLE].write = consolewrite;
  printf("consoleinit: console device registered\n");
}

// 供内核使用的字符输出 (保持不变)
void consputc(char c) {
  uart_putc(c);
}