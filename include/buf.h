// include/buf.h
#ifndef __BUF_H__
#define __BUF_H__

#include "riscv.h"
#include "fs.h"
struct buf {
  int valid;   // 数据是否已从磁盘读入?
  int disk;    // 内容是否已被修改(需要写回磁盘)?
  uint dev;    // 设备号
  uint blockno;// 块号
  uint refcnt; // 引用计数
  struct buf *prev; // LRU 链表
  struct buf *next;
  uchar data[BSIZE];
};

void binit(void);
struct buf* bread(uint, uint);
void brelse(struct buf*);
void bwrite(struct buf*);

#endif