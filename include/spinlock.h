// include/spinlock.h
#ifndef __SPINLOCK_H__
#define __SPINLOCK_H__

#include "riscv.h"

struct spinlock {
  uint locked;       // 是否被持有
  char *name;        // 锁名称(用于调试)
  struct cpu *cpu;   // 持有锁的 CPU
};

void initlock(struct spinlock *lk, char *name);
void acquire(struct spinlock *lk);
void release(struct spinlock *lk);

#endif