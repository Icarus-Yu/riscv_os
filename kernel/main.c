// kernel/main.c

#include "console.h"
#include "memory.h"
#include "trap.h"
#include "proc.h"
#include "buf.h" 
#include "fs.h"
#include "file.h"   // 提供 struct inode 定义
#include "stat.h"   // 提供 T_FILE 定义
#include "string.h" // 提供 memset 定义
#include "param.h"  // 提供 ROOTDEV 等定义

// 手动声明未在头文件中暴露的初始化函数
void virtio_disk_init(void);
struct inode* ialloc(uint dev, short type);
void iupdate(struct inode *ip);
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n);

// 来自 kernel/log.c
void begin_op(void);
void end_op(void);
void initlog(int dev, struct superblock *sb); // <--- [修复1] 声明 initlog
struct inode* namei(char *path);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iunlockput(struct inode *ip);
void iupdate(struct inode *ip);
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n);
// 这里推荐方案 1：临时暴露 create 函数供测试使用

struct inode* create(char *path, short type, short major, short minor);
// --- 【新增声明】为了实现 unlink 测试 ---
struct inode* nameiparent(char *path, char *name);
struct inode* dirlookup(struct inode *dp, char *name, uint *poff);
void test_filesystem() {
    printf("=== Testing File System (Advanced) ===\n");
    begin_op();
    
    // 1. 测试创建目录: 创建 /temp
    struct inode *dp = create("/temp", T_DIR, 0, 0);
    if(dp == 0) panic("failed to create /temp");
    iunlockput(dp);
    printf("[1] mkdir /temp: OK\n");

    // 2. 测试在目录下创建文件: 创建 /temp/hello
    struct inode *ip = create("/temp/hello", T_FILE, 0, 0);
    if(ip == 0) panic("failed to create /temp/hello");
    
    // 3. 写入数据
    char *msg = "Hello, RISC-V FS!";
    if(writei(ip, 0, (uint64)msg, 0, 18) != 18) panic("failed to write");
    printf("[2] write /temp/hello: OK\n");
    
    iunlockput(ip);
    end_op();

    // 4. 读取验证
    ip = namei("/temp/hello");
    if(ip == 0) panic("failed to find /temp/hello");
    ilock(ip);
    
    char buf[32];
    readi(ip, 0, (uint64)buf, 0, 18);
    printf("[3] read content: %s\n", buf);
    
    iunlockput(ip);

    // --- 【新增】测试删除文件 (模拟 unlink) ---
    printf("Testing unlink /temp/hello...\n");
    begin_op(); // 开启事务

    char name[DIRSIZ];
    uint off;
    struct dirent de;

    // 5.1 查找父目录 /temp
    dp = nameiparent("/temp/hello", name);
    if(dp == 0) panic("unlink: parent not found");
    ilock(dp);

    // 5.2 查找目标文件 hello
    ip = dirlookup(dp, name, &off);
    if(ip == 0) {
        iunlockput(dp);
        panic("unlink: file not found");
    }
    
    // 5.3 锁定目标文件
    ilock(ip);

    // 5.4 清空目录项 (即从父目录中移除该文件记录)
    memset(&de, 0, sizeof(de));
    if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
        panic("unlink: writei failed");
    
    // 5.5 减少文件的硬链接数
    ip->nlink--;
    iupdate(ip); // 更新 inode 到磁盘

    // 5.6 释放锁和引用 (iput 会检查 nlink，若为0则真正释放数据块)
    iunlockput(ip); // 释放文件 inode
    iunlockput(dp); // 释放父目录 inode

    end_op(); // 提交事务
    printf("[4] unlink /temp/hello: OK\n");

    // 6. 再次读取验证 (应该找不到文件)
    ip = namei("/temp/hello");
    if(ip != 0) {
        panic("unlink failed: file still exists");
    }
    printf("[5] verify unlink: OK (file gone)\n");
    
    printf("PASS: File System functionality check passed!\n");
}

void main() {
    // 1. 基础 UI 初始化
    clear_screen();
    printf("====== RISC-V OS Booting ======\n");

    // 2. 内存管理初始化 (物理内存 -> 内核页表 -> 开启分页)
    printf("[Boot] Initializing Memory...\n");
    kinit();         // 物理内存分配器
    kvminit();       // 创建内核页表
    kvminithart();   // 开启分页机制 (写入 satp)
    printf_color(COLOR_GREEN, " - Memory initialized.\n");

    // 3. 中断与时钟初始化
    printf("[Boot] Initializing Interrupts...\n");
    trapinit();      // 设置中断向量
    timerinit();     // 设置时钟中断
    printf_color(COLOR_GREEN, " - Interrupts initialized.\n");

    // 4. 文件系统与设备初始化 (实验 7 新增)
    printf("[Boot] Initializing File System...\n");
    virtio_disk_init(); // 初始化磁盘驱动
    binit();            // 初始化缓冲区缓存
    
    // [修复3] 初始化日志系统 (必须在 binit 之后，使用文件系统之前)
    struct buf *bp = bread(ROOTDEV, 1); // 读取超级块 (Block 1)
    struct superblock sb;
    memmove(&sb, bp->data, sizeof(sb));
    brelse(bp);
    initlog(ROOTDEV, &sb); // 初始化日志
    
    // iinit();         // (可选) 如果你实现了 inode 缓存初始化，可以在这里调用
    printf_color(COLOR_GREEN, " - File System initialized.\n");
    
    test_filesystem();
    
    // 5. 进程管理初始化
    printf("[Boot] Initializing Process Manager...\n");
    procinit();      // 初始化进程表
    userinit();      // 创建第一个用户进程 (initcode)
    printf_color(COLOR_GREEN, " - First user process created.\n");

    // 6. 开启中断并启动调度器
    printf("[Boot] System Ready. Handing over to scheduler...\n");
    printf("---------------------------------------------\n");
    
    intr_on();       // 开启全局中断
    scheduler();     // 进入调度循环 (永不返回)

    // 7. 死循环 (防御性编程，理论上永远不会执行到这里)
    panic("main: scheduler returned");
}