// kernel/syscall.c
#include "riscv.h"
#include "proc.h"
#include "syscall.h"
#include "console.h"
#include "string.h"
#include "memory.h"

// 辅助函数
int fetchaddr(uint64 addr, uint64 *ip) {
  struct proc *p = current_proc;
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(uint64)) != 0)
    return -1;
  return 0;
}

int fetchstr(uint64 addr, char *buf, int max) {
  struct proc *p = current_proc;
  int err = copyinstr(p->pagetable, buf, addr, max);
  if(err < 0)
    return -1;
  return strlen(buf);
}

// 参数获取函数
int argint(int n, int *ip) {
    struct proc *p = current_proc;
    struct trapframe *tf = p->trapframe;
    switch (n) {
    case 0: *ip = tf->a0; break;
    case 1: *ip = tf->a1; break;
    case 2: *ip = tf->a2; break;
    case 3: *ip = tf->a3; break;
    case 4: *ip = tf->a4; break;
    case 5: *ip = tf->a5; break;
    default: return -1;
    }
    return 0;
}

int argaddr(int n, uint64 *ip) {
    struct proc *p = current_proc;
    struct trapframe *tf = p->trapframe;
    switch (n) {
    case 0: *ip = tf->a0; break;
    case 1: *ip = tf->a1; break;
    case 2: *ip = tf->a2; break;
    default: return -1;
    }
    return 0;
}

// 外部系统调用声明
extern int sys_fork(void);
extern int sys_exit(void);
extern int sys_wait(void);
extern int sys_pipe(void);
extern int sys_read(void);
extern int sys_write(void);
extern int sys_close(void);
extern int sys_kill(void);
extern int sys_exec(void);
extern int sys_open(void);
extern int sys_mknod(void);
extern int sys_unlink(void);
extern int sys_fstat(void);
extern int sys_link(void);
extern int sys_mkdir(void);
extern int sys_chdir(void);
extern int sys_dup(void);
extern int sys_getpid(void);
extern int sys_sbrk(void);
extern int sys_sleep(void);
extern int sys_uptime(void);

// 系统调用表
static int (*syscalls[])(void) = {
    [SYS_fork]    sys_fork,
    [SYS_exit]    sys_exit,
    [SYS_wait]    sys_wait,
    // [SYS_pipe]    sys_pipe, // 暂时未实现
    [SYS_read]    sys_read,
    [SYS_write]   sys_write, // <--- 必须有
    [SYS_close]   sys_close,
    //[SYS_kill]    sys_kill,
    [SYS_exec]    sys_exec,  // <--- 必须有
    [SYS_open]    sys_open,  // <--- 必须有
    [SYS_mknod]   sys_mknod, // <--- 必须有，否则无法创建 console
    [SYS_unlink]  sys_unlink,
    [SYS_fstat]   sys_fstat,
    [SYS_link]    sys_link,
    [SYS_mkdir]   sys_mkdir,
    [SYS_chdir]   sys_chdir,
    [SYS_dup]     sys_dup,   // <--- 必须有
    [SYS_getpid]  sys_getpid,
    //[SYS_sbrk]    sys_sbrk,
    [SYS_sleep]   sys_sleep,
    // [SYS_uptime]  sys_uptime,
};

void syscall(void) {
    struct proc *p = current_proc;
    int num = p->trapframe->a7;

    if (num > 0 && num < sizeof(syscalls) / sizeof(syscalls[0]) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();
    } else {
        printf("pid %d %s: unknown sys call %d\n",
               p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}