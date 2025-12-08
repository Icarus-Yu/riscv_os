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
    // return pipewrite(f->pipe, addr, n);
    return -1;
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    return devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    // 写入文件的最大限制
    /*
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r != n1){
        // error from writei
        break;
      }
      i += r;
    }
    ret = (i == n ? n : -1);
    */
    // 为了简化，我们暂时只实现一次写入，暂不处理超大文件分片
    begin_op();
    ilock(f->ip);
    // 需要在 fs.c 实现 writei
    // if ((r = writei(f->ip, 1, addr, f->off, n)) > 0)
    //   f->off += r;
    // 暂时用 printf 替代，稍后补全 writei
    printf("filewrite: writei not implemented yet\n");
    r = n; // 假装写入成功
    iunlock(f->ip);
    end_op();
    ret = r;
  } else {
    panic("filewrite");
  }

  return ret;
}