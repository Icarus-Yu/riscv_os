TOOLCHAIN = riscv64-unknown-elf-
GDB = gdb-multiarch

CC = $(TOOLCHAIN)gcc
LD = $(TOOLCHAIN)ld
OBJCOPY = $(TOOLCHAIN)objcopy
OBJDUMP = $(TOOLCHAIN)objdump

# 编译选项
CFLAGS = -Wall -Werror -O0 -fno-omit-frame-pointer -ggdb -MD
CFLAGS += -ffreestanding -nostdlib -mno-relax -mcmodel=medany -march=rv64g
CFLAGS += -Iinclude -I. 

# 链接选项（仅用于内核）
LDFLAGS = -T scripts/kernel.ld -nostdlib

# --- 用户态程序设置 ---
U = user
# 用户库：包含系统调用桩、基础库、printf等
ULIB = $U/ulib.o $U/usys.o $U/printf.o

# 用户程序列表
UPROGS = \
	$U/init

# --- 源文件搜索路径 ---
# 显式地将 entry.S 分离出来，确保它在链接时是第一个
ENTRY_S = kernel/boot/entry.S

# 查找所有其他的 .S 文件 (排除 entry.S，且仅查找 kernel 目录)
SOURCES_S_OTHER = $(filter-out $(ENTRY_S), $(shell find kernel -name '*.S'))
SOURCES_C = $(shell find kernel -name '*.c')

# 转换 .o 文件
OBJECT_ENTRY = $(patsubst %.S, %.o, $(ENTRY_S))
OBJECTS_S_OTHER = $(patsubst %.S, %.o, $(SOURCES_S_OTHER))
OBJECTS_C = $(patsubst %.c, %.o, $(SOURCES_C))

# 确保 OBJECT_ENTRY (entry.o) 在链接顺序的最前面
OBJECTS = $(OBJECT_ENTRY) $(OBJECTS_S_OTHER) $(OBJECTS_C)
DEPS = $(patsubst %.o, %.d, $(OBJECTS))
TARGET_ELF = kernel/kernel.elf

# QEMU 运行选项
QEMU_OPTS = -machine virt -bios default -kernel $(TARGET_ELF) -nographic
QEMU_OPTS += -drive file=fs.img,if=none,format=raw,id=x0
QEMU_OPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

.PHONY: all clean qemu qemu-gdb debug

all: $(TARGET_ELF)

# --- 内核编译规则 ---
$(TARGET_ELF): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# --- 用户程序编译规则 ---

# 编译 usys.S (系统调用汇编桩)
$U/usys.o : $U/usys.S
	$(CC) $(CFLAGS) -c -o $U/usys.o $U/usys.S

# 链接 _init 程序
# 注意：用户程序不能使用内核的 LDFLAGS (kernel.ld)，需要手动指定 -Ttext 0
$U/init: $U/init.o $(ULIB)
	$(LD) -N -e main -Ttext 0 -nostdlib -o $@ $^
	$(OBJDUMP) -S $@ > $U/init.asm

# --- 清理规则 ---
clean:
	rm -rf kernel/kernel.elf $(shell find kernel -name '*.o' -o -name '*.d')
	rm -rf $(U)/*.o $(U)/*.d $(U)/*.asm $(U)/init
	rm -f fs.img mkfs/mkfs README

# --- 运行与调试 ---
qemu: $(TARGET_ELF) fs.img
	@echo "Starting QEMU..."
	@qemu-system-riscv64 $(QEMU_OPTS)

qemu-gdb: $(TARGET_ELF) fs.img
	@echo "Starting QEMU for GDB debugging..."
	@qemu-system-riscv64 $(QEMU_OPTS) -S -s

debug: $(TARGET_ELF) fs.img
	@tmux kill-session -t kernel_debug 2>/dev/null || true
	@tmux new-session -d -s kernel_debug "make qemu-gdb" \; \
		split-window -h "sleep 1; $(GDB) -ex 'target remote localhost:1234' $(TARGET_ELF)" \; \
		attach-session -t kernel_debug

-include $(DEPS)

# --- 文件系统镜像生成 ---

# 定义 mkfs 编译器 (使用宿主机的 gcc)
HOSTCC = gcc

fs.img: mkfs/mkfs $(UPROGS)
	@echo "Hello, RISC-V File System!" > README
	./mkfs/mkfs fs.img README $(UPROGS)
# 编译 mkfs（宿主机工具）
mkfs/mkfs: mkfs/mkfs.c include/types.h include/fs.h include/stat.h
	gcc -Wall -Werror -O0 -g -iquote include -std=gnu11 $< -o $@