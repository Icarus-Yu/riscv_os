
TOOLCHAIN = riscv64-unknown-elf-

CC = $(TOOLCHAIN)gcc
LD = $(TOOLCHAIN)ld
OBJCOPY = $(TOOLCHAIN)objcopy
OBJDUMP = $(TOOLCHAIN)objdump

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -MD
CFLAGS += -ffreestanding -nostdlib -mno-relax -mcmodel=medany

LDFLAGS = -T kernel/kernel.ld -nostdlib

SOURCES_S = $(wildcard kernel/*.S)
SOURCES_C = $(wildcard kernel/*.c)
OBJECTS_S = $(patsubst %.S, %.o, $(SOURCES_S))
OBJECTS_C = $(patsubst %.c, %.o, $(SOURCES_C))

# ！！！请务必确保这一行是正确的 ！！！
OBJECTS = $(OBJECTS_S) $(OBJECTS_C)

TARGET_ELF = kernel/kernel.elf

.PHONY: all clean qemu qemu-gdb

all: $(TARGET_ELF)

$(TARGET_ELF): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f kernel/*.o kernel/*.d $(TARGET_ELF)

qemu: $(TARGET_ELF)
	@echo "Starting QEMU..."
	@qemu-system-riscv64 \
		-machine virt \
		-bios none \
		-kernel $(TARGET_ELF) \
		-nographic

qemu-gdb: $(TARGET_ELF)
	@qemu-system-riscv64 \
		-machine virt \
		-bios none \
		-serial stdio \
		-kernel $(TARGET_ELF) \
		-S -gdb tcp::1234 \
		-nographic
