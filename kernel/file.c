// kernel/file.c
#include "types.h"
#include "riscv.h"
#include "fs.h"
#include "spinlock.h"
#include "file.h"
#include "param.h"
#include "proc.h"
#include "console.h"
#include "string.h"  // for memset
#include "stat.h"
// --- 手动添加外部函数声明 ---

// 来自 kernel/fs.c
void begin_op(void);
void end_op(void);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iput(struct inode *ip);
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n);

// 来自 kernel/mm/vm.c (用于 filestat)
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len);
struct devsw devsw[NDEV];

struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void fileinit(void) {
  initlock(&ftable.lock, "ftable");
  printf("fileinit: file table initialized\n");
}

// 分配一个文件结构体
struct file* filealloc(void) {
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// 增加引用计数
struct file* filedup(struct file *f) {
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// 关闭文件 (减少引用计数)
void fileclose(struct file *f) {
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  
  // 引用为0，真正回收
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE){
    // pipeclose(ff.pipe); // 暂时注释，实验8才会用到
  } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// 获取文件状态
int filestat(struct file *f, uint64 addr) {
  struct proc *p = current_proc;
  struct stat st;
  
  if(f->type == FD_INODE || f->type == FD_DEVICE){
    ilock(f->ip);
    // stati(f->ip, &st); // 需要在 fs.c 实现 stati
    // 这里简单手动填充
    st.dev = f->ip->dev;
    st.ino = f->ip->inum;
    st.type = f->ip->type;
    st.nlink = f->ip->nlink;
    st.size = f->ip->size;
    iunlock(f->ip);
    if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}

// 读文件
int fileread(struct file *f, uint64 addr, int n) {
  int r = 0;

  if(f->readable == 0)
    return -1;

  if(f->type == FD_PIPE){
    // return piperead(f->pipe, addr, n);
    return -1;
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    return devsw[f->major].read(1, addr, n);
  } else if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, 1, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
    return r;
  }
  panic("fileread");
  return -1;
}

// 写文件
int filewrite(struct file *f, uint64 addr, int n) {
  int r, ret = 0;

  if(f->writable == 0)
    return -1;

  if(f->type == FD_PIPE){
    // ...
    return -1;
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    return devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    // 使用事务保护写操作
    begin_op();
    ilock(f->ip);
    
    // 调用 fs.c 中的 writei
    if ((r = writei(f->ip, 0, addr, f->off, n)) > 0)
      f->off += r;
    
    ret = r;
    
    iunlock(f->ip);
    end_op();
  } else {
    panic("filewrite");
  }

  return ret;
}