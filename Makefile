TOOLCHAIN = riscv64-unknown-elf-
GDB = gdb-multiarch

CC = $(TOOLCHAIN)gcc
LD = $(TOOLCHAIN)ld
OBJCOPY = $(TOOLCHAIN)objcopy
OBJDUMP = $(TOOLCHAIN)objdump

CFLAGS = -Wall -Werror -O0 -fno-omit-frame-pointer -ggdb -MD
CFLAGS += -ffreestanding -nostdlib -mno-relax -mcmodel=medany
CFLAGS += -Iinclude

# --- 修改 LDFLAGS ---
# 链接脚本的路径已更改
LDFLAGS = -T scripts/kernel.ld -nostdlib

# --- 修改源文件搜索路径 ---
# 使用 find 命令来递归查找所有 .S 和 .c 文件，这样更灵活
SOURCES_S = $(shell find kernel -name '*.S')
SOURCES_C = $(shell find kernel -name '*.c')

OBJECTS_S = $(patsubst %.S, %.o, $(SOURCES_S))
OBJECTS_C = $(patsubst %.c, %.o, $(SOURCES_C))

OBJECTS = $(OBJECTS_S) $(OBJECTS_C)
DEPS = $(patsubst %.o, %.d, $(OBJECTS))
TARGET_ELF = kernel/kernel.elf

QEMU_OPTS = -machine virt -bios none -kernel $(TARGET_ELF) -nographic

.PHONY: all clean qemu qemu-gdb debug

all: $(TARGET_ELF)

$(TARGET_ELF): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

# --- 修改编译规则以处理不同目录下的文件 ---
# $< 会自动包含文件的相对路径 (例如 kernel/boot/entry.S)
# $@ 会根据 %.o 规则生成对应的 .o 文件 (例如 kernel/boot/entry.o)
# 这样就不需要为每个目录写单独的规则了
%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf kernel/kernel.elf $(shell find kernel -name '*.o' -o -name '*.d')

qemu: $(TARGET_ELF)
	@echo "Starting QEMU..."
	@qemu-system-riscv64 $(QEMU_OPTS)

qemu-gdb: $(TARGET_ELF)
	@echo "Starting QEMU for GDB debugging..."
	@qemu-system-riscv64 $(QEMU_OPTS) -S -s

debug: $(TARGET_ELF)
	@tmux kill-session -t kernel_debug 2>/dev/null || true
	@tmux new-session -d -s kernel_debug "make qemu-gdb" \; \
		split-window -h "sleep 1; $(GDB) -ex 'target remote localhost:1234' $(TARGET_ELF)" \; \
		attach-session -t kernel_debug

-include $(DEPS)
