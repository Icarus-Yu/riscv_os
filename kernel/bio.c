// kernel/bio.c
#include "buf.h"
#include "spinlock.h"
#include "console.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf head; // LRU 链表头
} bcache;

void virtio_disk_rw(struct buf *b, int write);

void binit(void) {
  struct buf *b;
  initlock(&bcache.lock, "bcache");

  // 初始化 LRU 双向循环链表
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    // 将 block 插入头部
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  printf("binit: buffer cache initialized.\n");
}

// 获取缓存块（如果不在缓存中则分配）
static struct buf* bget(uint dev, uint blockno) {
  struct buf *b;

  acquire(&bcache.lock);

  // 1. 检查是否已缓存
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      return b; // 锁由调用者管理 (sleeplock in xv6, here simplified)
    }
  }

  // 2. 未缓存，分配一个新的（使用 LRU 策略，从尾部找）
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      return b;
    }
  }
  
  panic("bget: no buffers");
  return 0;
}

// 读取磁盘块
struct buf* bread(uint dev, uint blockno) {
  struct buf *b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0); // 0 = read
    b->valid = 1;
  }
  return b;
}

// 写回磁盘块
void bwrite(struct buf *b) {
  virtio_disk_rw(b, 1); // 1 = write
}

// 释放缓存块
void brelse(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // 移到链表头部 (MRU)
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  release(&bcache.lock);
}