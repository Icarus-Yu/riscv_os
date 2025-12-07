// kernel/spinlock.c
#include "spinlock.h"
#include "console.h" // for panic

void initlock(struct spinlock *lk, char *name) {
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// 获取锁
void acquire(struct spinlock *lk) {
  // 在多核或开启中断的情况下，这里需要关闭中断
  // push_off(); // 暂时简化，假设单核且不关中断

  if(lk->locked)
      panic("acquire"); // 不允许重入

  // 核心原子操作：test-and-set
  // 循环直到成功交换(即原值为0)
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;
    
  // 内存屏障，保证临界区指令不被乱序执行到这里之前
  __sync_synchronize();
}

// 释放锁
void release(struct spinlock *lk) {
  if(!lk->locked)
    panic("release");

  __sync_synchronize();

  // 原子释放
  __sync_lock_release(&lk->locked);

  // pop_off();
}