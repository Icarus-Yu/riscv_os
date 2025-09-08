# 实验手册任务3：实现启动汇编代码

.section .text
.global _start

_start:
    # 任务1提到xv6的第一条指令就是设置栈指针
    # 我们也这样做，直接设置栈指针sp，不再需要gp的设置
    la sp, stack_top

    # 清零 BSS 段
    # 符号 bss_start 和 bss_end 将在链接脚本中定义
    la a0, bss_start
    la a1, bss_end
    bge a0, a1, .L_bss_done
.L_bss_loop:
    sd zero, (a0)
    addi a0, a0, 8
    blt a0, a1, .L_bss_loop
.L_bss_done:

    # 任务3.4: 跳转到C主函数
    call main

    # 防止程序意外退出，进入死循环
halt:
    wfi
    j halt

# 在 .bss 段中为启动栈预留空间
.section .bss
.align 16
stack_bottom:
.space 4096
stack_top:
