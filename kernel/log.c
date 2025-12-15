// kernel/log.c
#include "types.h"
#include "riscv.h"
#include "fs.h"
#include "buf.h"
#include "spinlock.h"
#include "proc.h"
#include "console.h" // 确保包含 panic 定义
#include "string.h"
// 简单的日志结构
struct logheader {
  int n;
  int block[LOGSIZE];
};

struct log {
  struct spinlock lock;
  int start;
  int size;
  int outstanding; // 正在执行的系统调用数量
  int committing;  // 是否正在提交
  int dev;
  struct logheader lh;
};

struct log log;

static void recover_from_log(void);
static void commit(void);

void initlog(int dev, struct superblock *sb) {
  if (sizeof(struct logheader) >= BSIZE)
    panic("initlog: too big logheader");

  initlock(&log.lock, "log");
  log.start = sb->logstart;
  log.size = sb->nlog;
  log.dev = dev;
  recover_from_log();
}

// 拷贝已提交的块从日志区到实际数据区
static void install_trans(int recovering) {
  int tail;

  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *lbuf = bread(log.dev, log.start + tail + 1); // 读日志区
    struct buf *dbuf = bread(log.dev, log.lh.block[tail]);   // 读目标区
    memmove(dbuf->data, lbuf->data, BSIZE);
    bwrite(dbuf);  // 写入实际位置
    if(!recovering) 
       // bpin(dbuf); // 模拟 xv6 的 bpin (虽简化版可能未实现 bpin，暂留空或忽略)
    brelse(lbuf);
    brelse(dbuf);
  }
}

// 读取日志头
static void read_head(void) {
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  log.lh.n = lh->n;
  for (i = 0; i < log.lh.n; i++) {
    log.lh.block[i] = lh->block[i];
  }
  brelse(buf);
}

// 写入日志头
static void write_head(void) {
  struct buf *buf = bread(log.dev, log.start);
  struct logheader *lh = (struct logheader *) (buf->data);
  int i;
  lh->n = log.lh.n;
  for (i = 0; i < log.lh.n; i++) {
    lh->block[i] = log.lh.block[i];
  }
  bwrite(buf);
  brelse(buf);
}

static void recover_from_log(void) {
  read_head();

  // --- 伪造/增强的展示逻辑 ---
  printf("log: checking recovery...\n");
  
  if (log.lh.n > 0) {
      // 如果日志头里有数据，说明上次崩溃了，需要恢复
      printf_color(COLOR_YELLOW, "log: recovering %d blocks from journal...\n", log.lh.n);
      
      install_trans(1); // 将日志块写回磁盘真实位置
      log.lh.n = 0;     // 清空日志计数
      write_head();     // 将清空后的日志头写回磁盘
      
      printf_color(COLOR_GREEN, "log: recovery complete.\n");
  } else {
      // 如果没有数据，说明上次是正常关闭（或者日志已提交）
      printf_color(COLOR_GREEN, "log: clean shutdown detected, no recovery needed.\n");
  }
}

// 开始事务
void begin_op(void) {
  acquire(&log.lock);
  while(1){
    if(log.committing){
      sleep(&log, &log.lock);
    } else if(log.lh.n + (log.outstanding + 1)*MAXOPBLOCKS > LOGSIZE){
      // 空间不够，等待
      sleep(&log, &log.lock);
    } else {
      log.outstanding += 1;
      release(&log.lock);
      break;
    }
  }
}

// 结束事务
void end_op(void) {
  int do_commit = 0;

  acquire(&log.lock);
  log.outstanding -= 1;
  if(log.committing)
    panic("log.committing");
  if(log.outstanding == 0){
    do_commit = 1;
    log.committing = 1;
  } else {
    // wakeup any waiter in begin_op()
    wakeup(&log);
  }
  release(&log.lock);

  if(do_commit){
    commit();
    acquire(&log.lock);
    log.committing = 0;
    wakeup(&log);
    release(&log.lock);
  }
}

// 将缓存块写入日志
static void write_log(void) {
  int tail;
  for (tail = 0; tail < log.lh.n; tail++) {
    struct buf *to = bread(log.dev, log.start + tail + 1); // log block
    struct buf *from = bread(log.dev, log.lh.block[tail]); // cache block
    memmove(to->data, from->data, BSIZE);
    bwrite(to); // write the log
    brelse(from);
    brelse(to);
  }
}

static void commit(void) {
  if (log.lh.n > 0) {
    write_log();     // Write modified blocks from cache to log
    write_head();    // Write header to disk -- the real commit
    install_trans(0); // Now write the data to real locations
    log.lh.n = 0;
    write_head();    // Erase the transaction from the log
  }
}

// 调用者修改了 b->data，调用此函数记录日志
void log_write(struct buf *b) {
  int i;

  acquire(&log.lock);
  if (log.lh.n >= LOGSIZE || log.lh.n >= log.size - 1)
    panic("too big a transaction");
  if (log.outstanding < 1)
    panic("log_write outside of trans");

  for (i = 0; i < log.lh.n; i++) {
    if (log.lh.block[i] == b->blockno)   // log absorption
      break;
  }
  log.lh.block[i] = b->blockno;
  if (i == log.lh.n) {  // Add new block to log?
    // bpin(b); // 简化版暂略
    log.lh.n++;
  }
  release(&log.lock);
}