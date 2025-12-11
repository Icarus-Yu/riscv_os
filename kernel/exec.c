// kernel/exec.c
#include "types.h"
#include "param.h"
#include "memory.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "file.h"
#include "elf.h"
#include "console.h"
#include "string.h"

// 声明外部函数
struct inode* namei(char *path);
void ilock(struct inode *ip);
void iunlockput(struct inode *ip);
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n);
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz); // 需要在 vm.c 确认是否有此函数，或稍后实现
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm); // 需确认
void proc_freepagetable(pagetable_t pagetable, uint64 sz); // 需确认

// 加载程序段到页表
static int loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz) {
  uint i, n;
  uint64 pa;

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i); // 需要在 vm.c 实现 walkaddr
    if(pa == 0)
      panic("loadseg: address should exist");
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, 0, (uint64)pa, offset+i, n) != n)
      return -1;
  }
  return 0;
}

int exec(char *path, char **argv) {
  char *s, *last;
  int i, off;
  uint64 argc, sz = 0, sp, ustack[MAXARG+1], stackbase;
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pagetable_t pagetable = 0, oldpagetable;
  struct proc *p = current_proc;

  begin_op();

  // 1. 查找可执行文件
  if((ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);

  // 2. 检查 ELF 魔数
  if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if(elf.magic != ELF_MAGIC)
    goto bad;

  // 3. 创建新页表
  if((pagetable = proc_pagetable(p)) == 0)
    goto bad;

  // 4. 加载程序段
  for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){
    if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    
    // 分配内存
    uint64 sz1;
    if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz, flags2perm(ph.flags))) == 0)
      goto bad;
    sz = sz1;
    
    // 读取数据到内存
    if(ph.vaddr % PGSIZE != 0)
      goto bad;
    if(loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  p = current_proc;
  uint64 oldsz = p->sz;

  // 5. 分配用户栈 (2个页，其中一个是保护页)
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if((sz1 = uvmalloc(pagetable, sz, sz + 2*PGSIZE, PTE_W)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz-2*PGSIZE); // 标记保护页无效 (guard page)
  sp = sz;
  stackbase = sp - PGSIZE;

  // 6. 推入参数字符串
  for(argc = 0; argv[argc]; argc++) {
    if(argc >= MAXARG)
      goto bad;
    sp -= strlen(argv[argc]) + 1;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if(sp < stackbase)
      goto bad;
    if(copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[argc] = sp;
  }
  ustack[argc] = 0;

  // 7. 推入 argv 数组
  sp -= (argc+1) * sizeof(uint64);
  sp -= sp % 16;
  if(sp < stackbase)
    goto bad;
  if(copyout(pagetable, sp, (char *)ustack, (argc+1)*sizeof(uint64)) < 0)
    goto bad;

  // 8. 准备 arguments for main(argc, argv)
  p->trapframe->a1 = sp; // argv

  // 9. 提交新状态
  oldpagetable = p->pagetable;
  p->pagetable = pagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry; // 初始程序计数器
  p->trapframe->sp = sp;         // 初始栈指针
  proc_freepagetable(oldpagetable, oldsz); // 释放旧页表

  return argc; // 返回 argc 到 a0

 bad:
  if(pagetable)
    proc_freepagetable(pagetable, sz);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

// 辅助函数：将 ELF 标志转换为 PTE 权限
int flags2perm(int flags) {
    int perm = 0;
    if(flags & 0x1) perm = PTE_X;
    if(flags & 0x2) perm |= PTE_W;
    return perm | PTE_R | PTE_U;
}