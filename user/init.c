// user/init.c
#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user/user.h"

int main(void) {
  //int i;
  //int pid, wpid;

  // 1. 初始化控制台
  // 如果 console 打开失败（比如还没创建设备节点），这里会尝试创建
  if(open("console", O_RDWR) < 0){
    mknod("console", 1, 1); // Major 1, Minor 1 是控制台
    open("console", O_RDWR);
  }
  
  // 2. 复制文件描述符，使 0, 1, 2 都指向 console
  dup(0);  // stdout
  dup(0);  // stderr

  printf("init: starting\n");

  // 3. 死循环，这就是我们的 Init 进程
  for(;;){
    printf("init: running...\n");
    sleep(100); // 睡眠 100 个 tick (约 10 秒)
    
    // 将来我们会在这里 fork 一个 shell
    // pid = fork();
    // if(pid < 0){
    //   printf("init: fork failed\n");
    //   exit(1);
    // }
    // if(pid == 0){
    //   exec("sh", argv);
    //   printf("init: exec sh failed\n");
    //   exit(1);
    // }
    // while((wpid=wait(0)) >= 0 && wpid != pid)
    //   printf("zombie!\n");
  }
}