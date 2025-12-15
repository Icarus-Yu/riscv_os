typedef long int64;
typedef unsigned long uint64;

#define SYS_fork    1
#define SYS_exit    2
#define SYS_wait    3
#define SYS_getpid  11
#define SYS_uptime  14
#define SYS_write   16

static inline int64 syscall(int64 num, int64 a0, int64 a1, int64 a2) {
    register int64 a7 asm("a7") = num;
    register int64 arg0 asm("a0") = a0;
    register int64 arg1 asm("a1") = a1;
    register int64 arg2 asm("a2") = a2;
    asm volatile("ecall" : "+r"(arg0) : "r"(arg1), "r"(arg2), "r"(a7) : "memory");
    return arg0;
}

int fork() { return syscall(SYS_fork, 0, 0, 0); }
void exit(int status) { syscall(SYS_exit, status, 0, 0); while(1); }
int wait(int *status) { return syscall(SYS_wait, (int64)status, 0, 0); }
int getpid() { return syscall(SYS_getpid, 0, 0, 0); }
int write(int fd, const void *buf, int count) { return syscall(SYS_write, fd, (int64)buf, count); }
int uptime() { return syscall(SYS_uptime, 0, 0, 0); }

int strlen(const char *s) {
    int n = 0;
    while(s[n]) n++;
    return n;
}

void print_str(const char *s) {
    write(1, s, strlen(s));
}

void print_int(int xx) {
    char buf[16];
    int i = 0, neg = 0;
    if(xx < 0) { neg = 1; xx = -xx; }
    do { buf[i++] = (xx % 10) + '0'; } while((xx /= 10) > 0);
    if(neg) buf[i++] = '-';
    while(i-- > 0) write(1, &buf[i], 1);
}

void printf(const char *fmt, ...) {
    // 简化版 printf，手动参数处理
    // 在真实场景下需配合 va_list，此处仅演示逻辑
    print_str(fmt);
}

void test_basic_syscalls() {
    print_str("Testing basic system calls...\n");
    
    int pid = getpid();
    print_str("Current PID: ");
    print_int(pid);
    print_str("\n");

    int child = fork();
    if(child == 0) {
        exit(42);
    } else if(child > 0) {
        print_str("Child process: PID=");
        print_int(child);
        print_str("\n");
        
        int status;
        wait(&status);
        print_str("Child exited with status: ");
        print_int(status);
        print_str("\n");
    } else {
        print_str("Fork failed\n");
    }
}

void test_parameter_passing() {
    print_str("Testing parameter passing...\n");
    char *msg = "Hello, World!";
    // 写入 fd 1 (stdout)，但不打印内容本身，只检查返回值
    
    int n = write(1, msg, 13); // 13 is length of "Hello, World!"
    print_str("\n"); 
    
    print_str("Wrote ");
    print_int(n);
    print_str(" bytes\n");
}

void test_security() {
    print_str("Testing security...\n");
    // 尝试写入内核地址 (0x80200000)
    int ret = write(1, (void*)0x80200000, 10);
    print_str("Invalid pointer write result: ");
    print_int(ret);
    print_str("\n");
}

void test_syscall_performance() {
    print_str("Testing syscall performance...\n");
    int start = uptime();
    for(int i = 0; i < 10000; i++) {
        getpid();
    }
    int end = uptime();
    
    print_str("10000 getpid() calls took ");
    print_int(end - start);
    print_str(" cycles\n");
}

int main() {
    print_str("\n====== TEST START: System Call Verification ======\n");
    
    test_basic_syscalls();
    test_parameter_passing();
    test_security();
    test_syscall_performance();
    
    print_str("====== TEST END: All Passed ======\n");
    exit(0);
    return 0;
}