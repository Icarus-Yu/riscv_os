// mkfs/mkfs.c
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>  // 必须包含，用于 memmove, memset, strncpy 等
#include <fcntl.h>
#include <assert.h>

#define stat xv6_stat  // 避免与宿主机 stat 结构体冲突
#include "types.h"     // 因为 Makefile 有 -Iinclude，所以直接写文件名即可
#include "fs.h"
#include "stat.h"
#undef stat

#ifndef static_assert
#define static_assert(a, b) do { switch (0) case 0: case (a): ; } while (0)
#endif

// 定义 min 宏
#define min(a, b) ((a) < (b) ? (a) : (b))

// 磁盘布局参数
#define NINODES 200

// 模拟磁盘相关
int fsfd;
struct superblock sb;
char zeroes[BSIZE];
uint freeinode = 1;
uint freeblock;

void wsect(uint, void*);
void winode(uint, struct dinode*);
void rinode(uint, struct dinode*);
void rsect(uint sec, void *buf);
uint ialloc(ushort type);
void iappend(uint inum, void *p, int n);

// 大小端转换 (对于 x86/RISC-V 这里其实是恒等变换)
ushort xshort(ushort x) { return x; }
uint xint(uint x) { return x; }

int main(int argc, char *argv[]) {
  int i, cc, fd;
  uint rootino, inum, off;
  struct dirent de;
  char buf[BSIZE];
  struct dinode din;

  static_assert(sizeof(int) == 4, "Integers must be 4 bytes!");

  if(argc < 2){
    fprintf(stderr, "Usage: mkfs fs.img files...\n");
    exit(1);
  }

  assert((BSIZE % sizeof(struct dinode)) == 0);
  assert((BSIZE % sizeof(struct dirent)) == 0);

  fsfd = open(argv[1], O_RDWR|O_CREAT|O_TRUNC, 0666);
  if(fsfd < 0){
    perror(argv[1]);
    exit(1);
  }

  // 1. 计算磁盘布局
  sb.magic = 0x10203040;
  sb.size = xint(1000); 
  sb.nlog = xint(LOGSIZE); 
  sb.ninodes = xint(NINODES);

  int nbitmap = FSSIZE / (BSIZE*8) + 1;
  int ninodeblocks = NINODES / (BSIZE / sizeof(struct dinode)) + 1;
  int nlog = LOGSIZE;  

  sb.logstart = xint(2); 
  sb.inodestart = xint(2 + nlog);
  sb.bmapstart = xint(2 + nlog + ninodeblocks);
  sb.nblocks = xint(FSSIZE - (2 + nlog + ninodeblocks + nbitmap));

  freeblock = 2 + nlog + ninodeblocks + nbitmap;

  printf("mkfs: super block write...\n");
  
  // 2. 初始化磁盘为 0
  for(i = 0; i < FSSIZE; i++)
    wsect(i, zeroes);

  // 3. 写入 Superblock
  memset(buf, 0, BSIZE);  // [修复] bzero -> memset
  memmove(buf, &sb, sizeof(sb));
  wsect(1, buf);

  // 4. 分配根目录 Inode
  rootino = ialloc(T_DIR); 
  assert(rootino == ROOTINO);

  // 写入目录项 "."
  memset(&de, 0, sizeof(de)); // [修复] bzero -> memset
  de.inum = xshort(rootino);
  strcpy(de.name, ".");
  iappend(rootino, &de, sizeof(de));

  // 写入目录项 ".."
  memset(&de, 0, sizeof(de)); // [修复] bzero -> memset
  de.inum = xshort(rootino);
  strcpy(de.name, "..");
  iappend(rootino, &de, sizeof(de));

  // 5. 写入其他文件
  for(i = 2; i < argc; i++){
    char *shortname;
    if(strncmp(argv[i], "user/", 5) == 0)
      shortname = argv[i] + 5;
    else
      shortname = argv[i];

    // [修复] index -> strchr
    assert(strchr(shortname, '/') == 0);

    if((fd = open(argv[i], 0)) < 0){
      perror(argv[i]);
      exit(1);
    }

    printf("mkfs: writing file '%s'\n", shortname);

    inum = ialloc(T_FILE); 

    memset(&de, 0, sizeof(de)); // [修复] bzero -> memset
    de.inum = xshort(inum);
    strncpy(de.name, shortname, DIRSIZ);
    iappend(rootino, &de, sizeof(de));

    while((cc = read(fd, buf, sizeof(buf))) > 0)
      iappend(inum, buf, cc);

    close(fd);
  }

  // 修复根目录大小
  rinode(rootino, &din);
  off = xint(din.size);
  off = ((off/BSIZE) + 1) * BSIZE;
  din.size = xint(off);
  winode(rootino, &din);

  // 写入位图 (简单版)
  uchar bitmap[BSIZE];
  memset(bitmap, 0, sizeof(bitmap)); // [修复] bzero -> memset
  uint bitblocks = freeblock; 
  for(i = 0; i < bitblocks; i++) {
     bitmap[i/8] |= (1 << (i%8));
  }
  wsect(sb.bmapstart, bitmap);

  close(fsfd);
  return 0;
}

void wsect(uint sec, void *buf){
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    perror("lseek");
    exit(1);
  }
  if(write(fsfd, buf, BSIZE) != BSIZE){
    perror("write");
    exit(1);
  }
}

void winode(uint inum, struct dinode *ip){
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = (inum / (BSIZE / sizeof(struct dinode))) + sb.inodestart;
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % (BSIZE / sizeof(struct dinode)));
  *dip = *ip;
  wsect(bn, buf);
}

void rinode(uint inum, struct dinode *ip){
  char buf[BSIZE];
  uint bn;
  struct dinode *dip;

  bn = (inum / (BSIZE / sizeof(struct dinode))) + sb.inodestart;
  rsect(bn, buf);
  dip = ((struct dinode*)buf) + (inum % (BSIZE / sizeof(struct dinode)));
  *ip = *dip;
}

void rsect(uint sec, void *buf){
  if(lseek(fsfd, sec * BSIZE, 0) != sec * BSIZE){
    perror("lseek");
    exit(1);
  }
  if(read(fsfd, buf, BSIZE) != BSIZE){
    perror("read");
    exit(1);
  }
}

uint ialloc(ushort type){
  uint inum = freeinode++;
  struct dinode din;

  memset(&din, 0, sizeof(din)); // [修复] bzero -> memset
  din.type = xshort(type);
  din.nlink = xshort(1);
  din.size = xint(0);
  winode(inum, &din);
  return inum;
}

void iappend(uint inum, void *xp, int n){
  char *p = (char*)xp;
  uint fbn, off, n1;
  struct dinode din;
  char buf[BSIZE];
  uint indirect[NINDIRECT];
  uint x;

  rinode(inum, &din);
  off = xint(din.size);
  
  while(n > 0){
    fbn = off / BSIZE;
    assert(fbn < MAXFILE);

    if(fbn < NDIRECT){
      if(din.addrs[fbn] == 0){
        din.addrs[fbn] = xint(freeblock++);
      }
      x = xint(din.addrs[fbn]);
    } else {
      if(din.addrs[NDIRECT] == 0){
        din.addrs[NDIRECT] = xint(freeblock++);
      }
      rsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      if(indirect[fbn - NDIRECT] == 0){
        indirect[fbn - NDIRECT] = xint(freeblock++);
        wsect(xint(din.addrs[NDIRECT]), (char*)indirect);
      }
      x = xint(indirect[fbn - NDIRECT]);
    }

    n1 = min(n, (fbn + 1) * BSIZE - off);
    rsect(x, buf);
    // [修复] bcopy -> memmove (注意参数顺序：dest, src, n)
    memmove(buf + off - (fbn * BSIZE), p, n1); 
    wsect(x, buf);
    n -= n1;
    off += n1;
    p += n1;
  }
  din.size = xint(off);
  winode(inum, &din);
}