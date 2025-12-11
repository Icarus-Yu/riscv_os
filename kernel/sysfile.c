// kernel/sysfile.c
#include "types.h"
#include "riscv.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "fcntl.h" // 需要定义 O_RDONLY 等
#include "console.h"
// --- 手动补充原本在 defs.h 中的函数声明 ---

// 来自 syscall.c
extern int argint(int n, int *ip);
extern int argaddr(int n, uint64 *ip);
// 你需要实现 argstr 来获取字符串参数 (文件名)
// 暂时可以用 argaddr + 简单的内核字符串读取替代，或者在 syscall.c 中实现 argstr
int argstr(int n, char *buf, int max); 

// 来自 fs.c
struct inode* namei(char *path);
void ilock(struct inode *ip);
void iunlock(struct inode *ip);
void iunlockput(struct inode *ip);
void iupdate(struct inode *ip);
void begin_op(void);
void end_op(void);

// 来自 file.c
struct file* filealloc(void);
struct file* filedup(struct file *f);
void fileclose(struct file *f);
int filestat(struct file *f, uint64 addr);
int fileread(struct file *f, uint64 addr, int n);
int filewrite(struct file *f, uint64 addr, int n);
int fdalloc(struct file *f); // 注意：你需要确保 file.c 中实现了这个函数，或者在这里实现

// ------------------------------------------
// 辅助函数：从文件描述符获取 file 结构
static int argfd(int n, int *pfd, struct file **pf) {
  int fd;
  struct file *f;
  if(argint(n, &fd) < 0) return -1;
  if(fd < 0 || fd >= NOFILE || (f=current_proc->ofile[fd]) == 0)
    return -1;
  if(pfd) *pfd = fd;
  if(pf) *pf = f;
  return 0;
}

// 1. 实现 sys_read
int sys_read(void) {
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

// 2. 实现 sys_write
int sys_write(void) {
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return filewrite(f, p, n);
}

// 3. 实现 sys_close
int sys_close(void) {
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  current_proc->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

// 4. 实现 sys_open (最核心)
int sys_open(void) {
  //char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;

  // 获取参数：path 和 mode
  // 注意：需要 argstr 函数 (fs.c 或 syscall.c 中需实现)
  // if(argstr(0, path, MAXPATH) < 0 || argint(1, &omode) < 0)
  //   return -1;
  // 这里简化：假设只获取地址
  uint64 pathaddr;
  if(argaddr(0, &pathaddr) < 0 || argint(1, &omode) < 0) return -1;
  // 此处应有 copyinstr 将路径从用户态拷贝到 path 数组

  begin_op();

  if(omode & O_CREATE){
    // create 逻辑需要 namei 支持，暂略，先支持打开已存在文件
    panic("sys_open: O_CREATE not fully supported");
  } else {
    if((ip = namei((char*)pathaddr)) == 0){ // 这里的 pathaddr 需要转换
       end_op();
       return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f) fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  return fd;
}

int fdalloc(struct file *f) {
  int fd;
  struct proc *p = current_proc;
  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}