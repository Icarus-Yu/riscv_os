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
// --- 新增宏定义 ---
// 一个位图块能包含多少个 bit (1024 * 8 = 8192)
#define BPB (BSIZE*8)

// 计算块 b 对应的位图块号 (bitmap block no)
// sb.bmapstart 是位图区的起始块号
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)
// 声明外部函数
extern void log_write(struct buf *b);
extern void begin_op(void);
extern void end_op(void);
// --- 新增下面这两个函数原型声明 ---
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n);
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
// int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n) {
//   uint tot, m;
//   struct buf *bp;

//   if(off > ip->size || off + n < off)
//     return 0;
//   if(off + n > ip->size)
//     n = ip->size - off;

//   for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
//     // 需要实现 bmap 来查找逻辑块对应的物理块
//     // 这里暂且只支持直接块 (前12个)
//     uint addr = ip->addrs[off/BSIZE]; 
//     if(addr == 0) break;
    
//     bp = bread(ip->dev, addr);
//     m = min(n - tot, BSIZE - off%BSIZE);
    
//     // 拷贝数据
//     // if(user_dst) copyout(...) else memmove(...)
//     // 简化：假设内核态
//     memmove((void*)dst, (char*)bp->data + (off%BSIZE), m);
//     brelse(bp);
//   }
//   return tot;
// }

// 路径解析 (最简版，只支持根目录下的文件查找)
struct inode* namei(char *path) {
  //char name[DIRSIZ];
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

// 将 inode 元数据写回磁盘
void iupdate(struct inode *ip) {
  struct buf *bp;
  struct dinode *dip;

  // 根据 inum 计算 inode 所在的磁盘块
  // 32 = boot(1) + super(1) + log(30), 需与 mkfs 布局一致
  uint block = 32 + ip->inum / (BSIZE / sizeof(struct dinode));
  bp = bread(ip->dev, block);
  
  // 定位到块内的具体 dinode
  dip = (struct dinode*)bp->data + ip->inum % (BSIZE / sizeof(struct dinode));
  
  // 更新字段
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  
  log_write(bp); // 记录到日志
  brelse(bp);
}

// 释放磁盘块（简化的 bitmap 操作，暂未实现 balloc/bfree，这里仅占位）
// 完整文件系统需要实现 balloc/bfree 来管理数据块位图
// static void bfree(int dev, uint b) {
//   struct buf *bp;
//   struct superblock sb;
//   int bi, m;

//   // 读取超级块
//   bp = bread(dev, 1);
//   memmove(&sb, bp->data, sizeof(sb));
//   brelse(bp);

//   // 读取对应的位图块
//   bp = bread(dev, BBLOCK(b, sb));
//   bi = b % BPB;
//   m = 1 << (bi % 8);
  
//   // 检查是否已经是空闲的
//   if((bp->data[bi/8] & m) == 0)
//     panic("freeing free block");
  
//   // 标记为 0 (空闲)
//   bp->data[bi/8] &= ~m;
//   log_write(bp);
//   brelse(bp);
// }

// 将块 b 的内容清零 (分配新块时必须清零，防止读取到垃圾数据)
static void bzero(int dev, int bno) {
  struct buf *bp;
  bp = bread(dev, bno);
  memset(bp->data, 0, BSIZE);
  log_write(bp);
  brelse(bp);
}
// 分配一个置零的磁盘块
static uint balloc(uint dev) {
  int b, bi, m;
  struct buf *bp;
  struct superblock sb;

  // 读取超级块以获取文件系统布局
  // 注意：为了简化，这里没有缓存 superblock，实际应该在 mount 时读取一次
  // 我们假设布局固定：
  // Block 0: unused
  // Block 1: super
  // ...
  // 但为了严谨，我们从磁盘读取 super block
  bp = bread(dev, 1);
  memmove(&sb, bp->data, sizeof(sb));
  brelse(bp);

  // 遍历所有数据块
  for(b = 0; b < sb.size; b += BPB){
    // 读取当前位图块
    bp = bread(dev, BBLOCK(b, sb));
    
    // 遍历位图块中的每一位
    for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
      m = 1 << (bi % 8);
      // 检查该位是否为 0
      if((bp->data[bi/8] & m) == 0){
        // 找到空闲块！标记为 1
        bp->data[bi/8] |= m;
        log_write(bp); // 记录日志
        brelse(bp);    // 释放位图块
        
        bzero(dev, b + bi); // 清零该数据块
        return b + bi;      // 返回块号
      }
    }
    brelse(bp);
  }
  
  printf("balloc: out of blocks\n");
  return 0;
}

// 核心函数：逻辑块号(bn) -> 物理块号(addr)
// 如果 alloc != 0，则在需要时分配新块
static uint bmap(struct inode *ip, uint bn) {
  uint addr, *a;
  struct buf *bp;

  // 1. 直接块 (Direct Block)
  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0){
      addr = balloc(ip->dev); // 分配新块
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  
  // 2. 间接块 (Indirect Block)
  bn -= NDIRECT;
  if(bn < NINDIRECT){
    // 检查一级间接表是否存在
    if((addr = ip->addrs[NDIRECT]) == 0){
      addr = balloc(ip->dev);
      ip->addrs[NDIRECT] = addr;
    }
    
    // 读取间接块
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    
    // 检查间接块中的条目
    if((addr = a[bn]) == 0){
      addr = balloc(ip->dev);
      a[bn] = addr;
      log_write(bp); // 间接块被修改，需记录日志
    }
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
  return 0;
}

// 完善 readi: 使用 bmap 读取数据
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n) {
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return 0;
  if(off + n > ip->size)
    n = ip->size - off;

  for(tot=0; tot<n; tot+=m, off+=m, dst+=m){
    uint addr = bmap(ip, off/BSIZE); // <--- 使用 bmap
    if(addr == 0) break;
    
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off%BSIZE);
    
    // 这里假设内核态，user_dst暂时忽略
    memmove((void*)dst, (char*)bp->data + (off%BSIZE), m);
    brelse(bp);
  }
  return tot;
}

// 实现 writei: 写入数据到 inode
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n) {
  uint tot, m;
  struct buf *bp;

  if(off > ip->size || off + n < off)
    return -1;
  // 文件大小增长限制（防止覆盖超级块等，简化处理）
  if(off + n > MAXFILE*BSIZE)
    return -1;

  for(tot=0; tot<n; tot+=m, off+=m, src+=m){
    uint addr = bmap(ip, off/BSIZE); // 获取或分配块
    if(addr == 0) break;
    
    bp = bread(ip->dev, addr);
    m = min(n - tot, BSIZE - off%BSIZE);
    
    // 写入数据
    memmove((char*)bp->data + (off%BSIZE), (void*)src, m);
    log_write(bp); // 标记脏块
    brelse(bp);
  }

  // 如果写入导致文件变大，更新大小
  if(n > 0){
    if(off > ip->size)
      ip->size = off;
    iupdate(ip); // 更新 inode 到磁盘
  }
  return tot;
}

void iunlockput(struct inode *ip) {
  iunlock(ip);
  iput(ip);
}