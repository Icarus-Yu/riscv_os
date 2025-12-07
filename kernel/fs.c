// kernel/fs.c
#include "types.h"
#include "riscv.h"
#include "fs.h"
#include "stat.h"
#include "buf.h"
#include "spinlock.h"
#include "proc.h"
#include "console.h"
#include "string.h"
// <--- 新增下面这两行 --->
#include "param.h" // 修复 "未定义标识符 NINODE"
#include "file.h"  // 修复 "不允许使用指向不完整类型 struct inode"
#define min(a, b) ((a) < (b) ? (a) : (b))

// 声明外部函数
extern void log_write(struct buf *b);
extern void begin_op(void);
extern void end_op(void);

// 内存中的 Inode 缓存
struct {
  struct spinlock lock;
  struct inode inode[NINODE];
} icache;

void iinit() {
  int i = 0;
  initlock(&icache.lock, "icache");
  for(i = 0; i < NINODE; i++) {
    // sleep lock init code ignored for simplicity
    // inithsleep(&icache.inode[i].lock, "inode");
  }
  printf("iinit: inode cache initialized\n");
}

static struct inode* iget(uint dev, uint inum);

// 分配一个新的 inode
struct inode* ialloc(uint dev, short type) {
  int inum;
  struct buf *bp;
  struct dinode *dip;

  for(inum = 1; inum < 200; inum++){ // 遍历 inode
    bp = bread(dev, 2 + 30 + inum / (BSIZE/sizeof(struct dinode))); // 计算块号 (hardcoded offsets for simplicity based on mkfs)
    // 更好的做法是读取 superblock，这里简化处理，假设 superblock 已读或用常量
    // 实际上我们在 mkfs 里写死了布局，这里要对应。
    // Boot(1) + Super(1) + Log(30) = 32. Inode start at 32.
    // 严谨写法应该读取 Superblock。稍后补充 read_sb。
    
    dip = (struct dinode*)(bp->data) + inum % (BSIZE/sizeof(struct dinode));
    if(dip->type == 0){ // Found free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp);   // mark it allocated on disk
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  panic("ialloc: no inodes");
  return 0;
}

// 将 inode 从磁盘读入内存
void ilock(struct inode *ip) {
  struct buf *bp;
  struct dinode *dip;

  if(ip == 0 || ip->ref < 1)
    panic("ilock");

  // acquiresleep(&ip->lock); // 简化：假设单线程访问，暂不加 sleep lock

  if(ip->valid == 0){
    // 从磁盘读取
    // 必须与 mkfs 布局一致：Boot(1)+Super(1)+Log(30) = 32
    uint block = 32 + ip->inum / (BSIZE / sizeof(struct dinode));
    bp = bread(ip->dev, block);
    dip = (struct dinode*)bp->data + ip->inum % (BSIZE / sizeof(struct dinode));
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    brelse(bp);
    ip->valid = 1;
    if(ip->type == 0)
      panic("ilock: no type");
  }
}

// 释放 inode 内存引用
void iput(struct inode *ip) {
  acquire(&icache.lock);

  if(ip->ref == 1 && ip->valid && ip->nlink == 0){
    // 文件已被删除且无引用，应该释放磁盘空间
    // 这里简化，暂不实现 truncate
    ip->valid = 0;
  }

  ip->ref--;
  release(&icache.lock);
}

// 查找/增加引用 inode
static struct inode* iget(uint dev, uint inum) {
  struct inode *ip, *empty;

  acquire(&icache.lock);

  // 1. 查找是否已在缓存
  empty = 0;
  for(ip = &icache.inode[0]; ip < &icache.inode[NINODE]; ip++){
    if(ip->ref > 0 && ip->dev == dev && ip->inum == inum){
      ip->ref++;
      release(&icache.lock);
      return ip;
    }
    if(empty == 0 && ip->ref == 0)    // Remember empty slot
      empty = ip;
  }

  // 2. 分配新缓存项
  if(empty == 0)
    panic("iget: no inodes");

  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&icache.lock);

  return ip;
}

void iunlock(struct inode *ip) {
  // releasesleep(&ip->lock);
}

// 简化版：从 inode 读取数据
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n) {
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return 0;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    // 需要实现 bmap 来查找逻辑块对应的物理块
    // 这里暂且只支持直接块 (前12个)
    uint addr = ip->addrs[off/BSIZE]; 
    if(addr == 0) break;
    
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off%BSIZE);
    
    // 拷贝数据
    // if(user_dst) copyout(...) else memmove(...)
    // 简化：假设内核态
    memmove((void*)dst, (char*)bp->data + (off%BSIZE), m);
    brelse(bp);
  }
  return tot;
}

// 路径解析 (最简版，只支持根目录下的文件查找)
struct inode* namei(char *path) {
  char name[DIRSIZ];
  struct inode *dp;
  struct dirent de;
  
  if(*path == '/') path++;
  
  // 硬编码：只支持根目录
  dp = iget(1, ROOTINO); // dev 1, root inode
  ilock(dp);

  // 遍历目录项
  for(uint off=0; off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      break;
    if(de.inum == 0) continue;
    if(strncmp(path, de.name, DIRSIZ) == 0){
      // Found
      iunlock(dp);
      // iput(dp); // Don't put root yet
      return iget(dp->dev, de.inum);
    }
  }
  
  iunlock(dp);
  iput(dp);
  return 0;
}