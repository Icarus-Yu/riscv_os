#ifndef __PARAM_H__
#define __PARAM_H__

#define NPROC        64  // 最大进程数 (proc.h 中也有定义，稍后统一)
#define NCPU          1  // 最大 CPU 数
#define NOFILE       16  // 每个进程最大打开文件数
#define NFILE       100  // 系统总共最大打开文件数
#define NINODE       50  // Inode 缓存的最大数量
#define NDEV         10  // 最大设备号
#define ROOTDEV       1  // 根文件系统设备号

#endif