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
void iupdate(struct inode *ip);
void iunlock(struct inode *ip);
void itrunc(struct inode *ip);
static void bfree(int dev, uint b);
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
struct inode* idup(struct inode *ip) {
  if(ip == 0) return 0;
  acquire(&icache.lock);
  ip->ref++;
  release(&icache.lock);
  return ip;
}

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
// 释放 inode 内存引用 (完善版)
void iput(struct inode *ip) {
  acquire(&icache.lock);

  if(ip->ref == 1 && ip->valid && ip->nlink == 0){
    // 如果没有引用且链接数为0，说明文件已被删除
    // 需要释放其占用的磁盘块
    
    // 必须释放锁，因为 itrunc -> bread 会睡眠
    release(&icache.lock);

    // 假设调用者（如 sys_unlink）已经开启了事务
    // 或者 iput 被 fileclose 调用时也开启了事务
    // 这里我们直接进行操作
    
    ilock(ip);
    itrunc(ip); // <--- 这里调用了 itrunc，从而使用了 bfree
    ip->type = 0;
    iupdate(ip);
    ip->valid = 0;
    iunlock(ip);

    acquire(&icache.lock);
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
// struct inode* namei(char *path) {
//   //char name[DIRSIZ];
//   struct inode *dp;
//   struct dirent de;
  
//   if(*path == '/') path++;
  
//   // 硬编码：只支持根目录
//   dp = iget(1, ROOTINO); // dev 1, root inode
//   ilock(dp);

//   // 遍历目录项
//   for(uint off=0; off<dp->size; off+=sizeof(de)){
//     if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
//       break;
//     if(de.inum == 0) continue;
//     if(strncmp(path, de.name, DIRSIZ) == 0){
//       // Found
//       iunlock(dp);
//       // iput(dp); // Don't put root yet
//       return iget(dp->dev, de.inum);
//     }
//   }
  
//   iunlock(dp);
//   iput(dp);
//   return 0;
// }

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
// 释放磁盘块
static void bfree(int dev, uint b) {
  struct buf *bp;
  struct superblock sb;
  int bi, m;

  // 1. 读取超级块以获取布局信息
  bp = bread(dev, 1);
  memmove(&sb, bp->data, sizeof(sb));
  brelse(bp);

  // 2. 读取对应的位图块
  // BBLOCK 宏计算块 b 对应的位图块号
  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;      // 在该位图块内的位索引
  m = 1 << (bi % 8); // 对应的掩码

  // 3. 检查是否已经是空闲的 (防止重复释放)
  if((bp->data[bi/8] & m) == 0)
    panic("freeing free block");

  // 4. 标记为 0 (空闲)
  bp->data[bi/8] &= ~m;
  
  // 5. 记录日志并释放缓存
  log_write(bp);
  brelse(bp);
}

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
// 将 inode 的内容截断（清空）
// 调用者必须持有 ip->lock
void itrunc(struct inode *ip) {
  int i, j;
  struct buf *bp;
  uint *a;

  // 1. 释放直接块
  for(i = 0; i < NDIRECT; i++){
    if(ip->addrs[i]){
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  // 2. 释放间接块
  if(ip->addrs[NDIRECT]){
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint*)bp->data;
    for(j = 0; j < NINDIRECT; j++){
      if(a[j])
        bfree(ip->dev, a[j]);
    }
    brelse(bp);
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  // 3. 更新大小并写回磁盘
  ip->size = 0;
  iupdate(ip);
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


// 在目录 dp 中查找名为 name 的文件
// 如果找到，返回该文件的 inode (已 iget)，并设置 *poff 为目录项偏移量
struct inode* dirlookup(struct inode *dp, char *name, uint *poff) {
  uint off, inum;
  struct dirent de;

  if(dp->type != T_DIR)
    panic("dirlookup not DIR");

  // 遍历目录数据块
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    
    if(de.inum == 0)
      continue;
      
    // 比较文件名 (需要 string.h 中的 strncmp)
    if(strncmp(name, de.name, DIRSIZ) == 0){
      // 找到了！
      if(poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0; // 没找到
}


// 将 (name, inum) 写入目录 dp
int dirlink(struct inode *dp, char *name, uint inum) {
  int off;
  struct dirent de;
  struct inode *ip;

  // 1. 检查文件名是否已存在
  if((ip = dirlookup(dp, name, 0)) != 0){
    iput(ip);
    return -1;
  }

  // 2. 寻找空的目录项槽位
  for(off = 0; off < dp->size; off += sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if(de.inum == 0)
      break;
  }

  // 3. 写入新的目录项
  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("dirlink");

  return 0;
}

// 辅助函数：从路径中提取下一个元素
// 例如：path="/a/b", name="a", 返回 path="/b"
static char* skipelem(char *path, char *name) {
  char *s;
  int len;

  while(*path == '/')
    path++;
  if(*path == 0)
    return 0;
  
  s = path;
  while(*path != '/' && *path != 0)
    path++;
  
  len = path - s;
  if(len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else {
    memmove(name, s, len);
    name[len] = 0;
  }
  
  while(*path == '/')
    path++;
  return path;
}

// 核心路径查找函数
// 如果 nameiparent 为真，则返回路径中最后一个元素的父目录 inode，并将最后一个元素名复制到 name
// 如果 nameiparent 为假，则返回路径中最后一个元素的 inode
static struct inode* namex(char *path, int nameiparent, char *name) {
  struct inode *ip, *next;

  // 1. 确定起始目录
  if(*path == '/')
    ip = iget(ROOTDEV, ROOTINO); // 绝对路径，从根目录开始
  else
    ip = idup(current_proc->cwd); // 相对路径，从当前工作目录开始 (需在 proc.h 中确认 cwd 字段)

  while((path = skipelem(path, name)) != 0){
    // 2. 锁定当前目录 inode
    ilock(ip);

    // 3. 检查是否为目录
    if(ip->type != T_DIR){
      iunlockput(ip);
      return 0;
    }

    // 4. 如果是查找父目录，且已到达最后一个元素，则停止
    if(nameiparent && *path == '\0'){
      iunlock(ip); // 返回锁定的 inode (但不持有锁，由调用者处理? xv6通常返回解锁的inode或上锁的? 
                   // xv6标准：nameiparent返回unlock的inode, namei返回locked的inode?
                   // 纠正：xv6 namex 返回的 ip 是解锁的 (iget状态)，调用者决定是否 lock。
                   // 但为了方便，我们通常在循环里 lock check 之后 unlock。
                   // 这里的逻辑：Stop one level early.
      return ip;
    }

    // 5. 在目录中查找下一级
    if((next = dirlookup(ip, name, 0)) == 0){
      iunlockput(ip);
      return 0;
    }

    iunlockput(ip); // 释放当前级，移动到下一级
    ip = next;
  }

  if(nameiparent){
    iput(ip);
    return 0;
  }

  return ip;
}

// 外部接口：解析路径，返回对应的 inode
struct inode* namei(char *path) {
  char name[DIRSIZ];
  return namex(path, 0, name);
}

// 外部接口：解析路径，返回父目录 inode，并将文件名填入 name
struct inode* nameiparent(char *path, char *name) {
  return namex(path, 1, name);
}