# kernel/trap/trampoline.S

.section .text
.global uservec
.global userret

# -------------------------------------------------------------------
# uservec: 用户态 -> 内核态 的入口
# 当用户程序执行 ecall 或发生中断时，CPU 会跳转到这里
# -------------------------------------------------------------------
uservec:
    # 此时：
    # sscratch 指向 trapframe (在 usertrapret 中设置)
    # a0 包含用户程序的第一个参数 (或者系统调用返回值的位置)

    # 1. 交换 a0 和 sscratch
    #    交换后：a0 = trapframe 地址, sscratch = 用户程序原来的 a0
    csrrw a0, sscratch, a0

    # 2. 将用户寄存器保存到 trapframe 中
    #    trapframe 的结构定义在 include/proc.h
    sd ra, 40(a0)
    sd sp, 48(a0)
    sd gp, 56(a0)
    sd tp, 64(a0)
    sd t0, 72(a0)
    sd t1, 80(a0)
    sd t2, 88(a0)
    sd s0, 96(a0)
    sd s1, 104(a0)
    sd a1, 120(a0)
    sd a2, 128(a0)
    sd a3, 136(a0)
    sd a4, 144(a0)
    sd a5, 152(a0)
    sd a6, 160(a0)
    sd a7, 168(a0)
    sd s2, 176(a0)
    sd s3, 184(a0)
    sd s4, 192(a0)
    sd s5, 200(a0)
    sd s6, 208(a0)
    sd s7, 216(a0)
    sd s8, 224(a0)
    sd s9, 232(a0)
    sd s10, 240(a0)
    sd s11, 248(a0)
    sd t3, 256(a0)
    sd t4, 264(a0)
    sd t5, 272(a0)
    sd t6, 280(a0)

    # 保存用户原来的 a0 (现在在 sscratch 中)
    csrr t0, sscratch
    sd t0, 112(a0)

    # 3. 恢复内核环境
    # 从 trapframe 中加载内核栈指针、内核页表、当前 CPU ID 等

    # 加载内核栈指针 (kernel_sp)
    ld sp, 8(a0)
    # 加载当前 CPU ID (kernel_hartid)
    ld tp, 32(a0)
    # 加载 usertrap 函数地址 (kernel_trap)
    ld t0, 16(a0)

    # 注意：如果你的 OS 实现了独立的用户页表，这里需要切换 satp
     ld t1, 0(a0)      # 加载 kernel_satp
     csrw satp, t1     # 切换到内核页表
     sfence.vma zero, zero

    # 4. 跳转到 C 语言处理函数 usertrap()
    jr t0

# -------------------------------------------------------------------
# userret: 内核态 -> 用户态 的出口
# C 语言函数 usertrapret 会调用这里
# -------------------------------------------------------------------
userret:
    # 参数：
    # a0: trapframe 的物理地址
    # a1: 用户页表 (satp 值) - 暂时未用，因为还没做用户页表隔离

    # 1. 切换页表 (如果需要)
     csrw satp, a1
     sfence.vma zero, zero

    # 2. 将 trapframe 地址放入 sscratch
    #    这样下次进入 uservec 时能找到它
    csrw sscratch, a0

    # 3. 从 trapframe 恢复用户寄存器
    ld ra, 40(a0)
    ld sp, 48(a0)
    ld gp, 56(a0)
    ld tp, 64(a0)
    ld t0, 72(a0)
    ld t1, 80(a0)
    ld t2, 88(a0)
    ld s0, 96(a0)
    ld s1, 104(a0)
    ld a1, 120(a0)
    ld a2, 128(a0)
    ld a3, 136(a0)
    ld a4, 144(a0)
    ld a5, 152(a0)
    ld a6, 160(a0)
    ld a7, 168(a0)
    ld s2, 176(a0)
    ld s3, 184(a0)
    ld s4, 192(a0)
    ld s5, 200(a0)
    ld s6, 208(a0)
    ld s7, 216(a0)
    ld s8, 224(a0)
    ld s9, 232(a0)
    ld s10, 240(a0)
    ld s11, 248(a0)
    ld t3, 256(a0)
    ld t4, 264(a0)
    ld t5, 272(a0)
    ld t6, 280(a0)

    # 最后恢复 a0 (因为它用于传递参数 a0，所以必须最后恢复)
    ld a0, 112(a0)

    # 4. 返回用户态
    sret
