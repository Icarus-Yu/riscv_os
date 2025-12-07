// include/fs.h
#ifndef __FS_H__
#define __FS_H__

#include "riscv.h"

// 磁盘布局相关常量
#define BSIZE 1024        // 块大小 (Block Size)
#define MAXOPBLOCKS 10    // 每次事务最大涉及块数
#define LOGSIZE (MAXOPBLOCKS*3) // 日志区大小
#define NBUF (MAXOPBLOCKS*3)    // 内存中的缓存块数量

#define FSSIZE 1000       // 文件系统总大小 (块数)

// 磁盘布局：
// [ boot block | super block | log | inode blocks | free bit map | data blocks ]

#define ROOTINO 1         // 根目录的 Inode 编号
#define BSIZE 1024        // 块大小

// 超级块 (Superblock)
struct superblock {
  uint magic;      // 魔数，用于校验文件系统类型
  uint size;       // 文件系统总块数
  uint nblocks;    // 数据块数量
  uint ninodes;    // Inode 数量
  uint nlog;       // 日志块数量
  uint logstart;   // 日志区起始块号
  uint inodestart; // Inode 区起始块号
  uint bmapstart;  // 位图起始块号
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// 磁盘上的 Inode 结构 (On-disk Inode)
struct dinode {
  short type;           // 文件类型 (文件/目录/设备)
  short major;          // 主设备号 (如果是设备文件)
  short minor;          // 次设备号
  short nlink;          // 硬链接计数
  uint size;            // 文件大小 (字节)
  uint addrs[NDIRECT+1]; // 数据块地址 (12个直接 + 1个间接)
};

// 目录项结构
#define DIRSIZ 14
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

#endif