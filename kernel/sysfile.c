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
#include "string.h"
// --- 手动补充原本在 defs.h 中的函数声明 ---
// 【新增】补充 readi, writei 和 isdirempty 的声明
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n);
int isdirempty(struct inode *dp); // 前向声明
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
struct inode* nameiparent(char *path, char *name); // <--- 新增
struct inode* dirlookup(struct inode *dp, char *name, uint *poff); // <--- 新增
struct inode* ialloc(uint dev, short type); // <--- 新增
int dirlink(struct inode *dp, char *name, uint inum); // <--- 新增
void iput(struct inode *ip); // <--- 新增
// 来自 file.c
struct file* filealloc(void);
struct file* filedup(struct file *f);
void fileclose(struct file *f);
int filestat(struct file *f, uint64 addr);
int fileread(struct file *f, uint64 addr, int n);
int filewrite(struct file *f, uint64 addr, int n);
int fdalloc(struct file *f); // 注意：你需要确保 file.c 中实现了这个函数，或者在这里实现

// ------------------------------------------
//在文件系统中创建一个新节点，目录或文件
 struct inode* create(char *path, short type, short major, short minor) {
  struct inode *ip, *dp;
  char name[DIRSIZ];

  // 1. 找到父目录
  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  // 2. 检查文件名是否已存在
  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip; // 文件已存在，直接返回
    iunlockput(ip);
    return 0;
  }

  // 3. 分配新 inode
  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip); // 写回磁盘

  // 4. 处理目录的特殊情况 (. 和 ..)
  if(type == T_DIR){
    // 目录的链接数初始为2 (父目录指向它，它自己的 . 指向它)
    // 但 xv6 简化处理，这里 dirlink 会处理名字
    // 此处 Create a new directory implies creating "." and ".." entries? 
    // 简化版：可以先不处理 . 和 ..，只处理 dirlink
    // 标准做法：
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  // 5. 将新 inode 链接到父目录
  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);
  return ip;
}
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
   ip = create((char*)pathaddr, T_FILE, 0, 0); // 使用 create 创建文件
    if(ip == 0){
      end_op();
      return -1;
    }
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

int sys_mkdir(void) {
  //char path[MAXPATH];
  struct inode *ip;

  begin_op();
  // 注意：需要 argstr 实现，如果没有实现，暂时只能传地址强转
  // if(argstr(0, path, MAXPATH) < 0) ...
  // 临时方案：假设 a0 是地址
  uint64 pathaddr;
  argaddr(0, &pathaddr);
  
  if((ip = create((char*)pathaddr, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

// kernel/sysfile.c

int sys_chdir(void) {
  //char path[MAXPATH];
  struct inode *ip;
  struct proc *p = current_proc;
  
  begin_op();
  // 同上，临时获取参数
  uint64 pathaddr;
  argaddr(0, &pathaddr);

  if((ip = namei((char*)pathaddr)) == 0){
    end_op();
    return -1;
  }
  
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  
  // 切换进程的当前目录
  iput(p->cwd);
  p->cwd = ip;
  
  end_op();
  return 0;
}

// kernel/sysfile.c


// --- 新增：创建硬链接 ---
int sys_link(void) {
  char name[DIRSIZ];
  struct inode *dp, *ip;
  // 临时使用 argaddr 获取字符串地址 (假设内核可以直接读取用户指针)
  // 标准做法应使用 argstr
  uint64 oldpath, newpath;

  if(argaddr(0, &oldpath) < 0 || argaddr(1, &newpath) < 0)
    return -1;
  
  // 1. 查找源文件
  begin_op();
  if((ip = namei((char*)oldpath)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip); // 更新链接数
  iunlock(ip);

  // 2. 查找目标目录
  if((dp = nameiparent((char*)newpath, name)) == 0)
    goto bad;
  
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip); // 释放源文件引用

  end_op();
  return 0;

bad:
  ilock(ip);
  ip->nlink--; // 回滚链接数
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// --- 新增：删除文件/取消链接 ---
int sys_unlink(void) {
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ];
  uint off;
  uint64 pathaddr;

  if(argaddr(0, &pathaddr) < 0)
    return -1;

  begin_op();
  
  // 1. 查找父目录
  if((dp = nameiparent((char*)pathaddr, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // 2. 确保不能删除 "." 和 ".."
  if(strncmp(name, ".", DIRSIZ) == 0 || strncmp(name, "..", DIRSIZ) == 0)
    goto bad;

  // 3. 在目录中查找目标文件
  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  // 4. 清空目录项
  memset(&de, 0, sizeof(de));
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp); // 释放父目录

  // 5. 减少目标文件的链接数
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip); // 这里会触发 iput -> itrunc (如果 nlink==0)

  end_op();
  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

// 辅助函数：检查目录是否为空 (除了 . 和 ..)
// 你可能需要在 sysfile.c 头部声明它，或者直接放在 sys_unlink 前面
int isdirempty(struct inode *dp) {
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}