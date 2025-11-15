# kernel/proc/swtch.S
# 对应手册 5.4 节 "上下文切换机制" [cite: 1158-1168, 3931, 3932]

.section .text
.global swtch   # 声明 swtch 为全局符号，C 代码可以调用

# void swtch(struct context *old, struct context *new);
# a0: (struct context *) old
# a1: (struct context *) new
swtch:
    # --- 保存 old 上下文 ---
    # 我们需要将 14 个寄存器保存到 old 指向的内存中
    # 偏移量必须严格匹配 include/proc.h 中的 struct context

    sd ra, 0(a0)     # 保存 old 的返回地址 (ra)
    sd sp, 8(a0)     # 保存 old 的栈指针 (sp)
    sd s0, 16(a0)    # 保存 old 的 s0-s11 寄存器
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)

    # --- 恢复 new 上下文 ---
    # 我们需要从 new 指向的内存中，将 14 个寄存器加载到 CPU

    ld ra, 0(a1)     # 加载 new 的返回地址 (ra)
    ld sp, 8(a1)     # 加载 new 的栈指针 (sp)
    ld s0, 16(a1)    # 加载 new 的 s0-s11 寄存器
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)

    ret             # 返回
