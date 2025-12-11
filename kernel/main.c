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

void test_large_file() {
    printf("=== Testing Large File ===\n");
    
    // [修复2] ialloc 涉及磁盘写操作，必须包含在事务(begin_op/end_op)中
    begin_op();
    // 1. 获取一个空闲 Inode
    struct inode *ip = ialloc(ROOTDEV, T_FILE); 
    end_op(); // 提交 ialloc 的修改

    // 检查是否分配成功
    if(ip == 0) {
        panic("test_large_file: ialloc failed");
    }
    
    char buf[BSIZE];
    memset(buf, 'A', BSIZE);
    
    // 【关键修复】拆分事务：每次只写 1 个块 (1KB)
    // 这样每个 begin_op/end_op 只涉及 1个数据块+1个位图块+1个inode块 < 10 (MAX)
    for(int i = 0; i < 16; i++) {
        begin_op();
        writei(ip, 0, (uint64)buf, i * BSIZE, BSIZE);
        iupdate(ip); // 更新 inode 大小
        end_op();
        printf("."); // 打印进度点
    }
    printf("\n");
    
    printf("Large file size: %d bytes (Expected 16384)\n", ip->size);
    if(ip->size == 16384) 
        printf("PASS: Large file write success!\n");
    else 
        printf("FAIL: Size mismatch.\n");
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
    
    test_large_file();
    
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