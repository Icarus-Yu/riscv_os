#include "console.h"
#include "memory.h"
#include "trap.h"

void main() {
    clear_screen();
    printf("====== RISC-V OS Booting ======\n");
    
    printf("\n====== Experiment 2: Printf Test ======\n");
    printf("Testing integer: %d\n", 12345);
    printf("Testing negative: %d\n", -54321);
    printf("Testing hex: 0x%x\n", 0xABCD);
    printf("Testing string: %s\n", "Hello, OS!");
    printf("Testing pointer: %p\n", (void*)0x80200000);
    
    printf("\n====== Experiment 3: Memory Management ======\n");
    kinit();
    kvminit();
    kvminithart();
    printf_color(COLOR_GREEN, "Virtual memory enabled!\n");
    
    printf("\n====== Experiment 4: Interrupt & Timer ======\n");
    trapinit();
    timerinit();
    
    intr_on();
    printf_color(COLOR_YELLOW, "Interrupts enabled! Waiting for timer...\n\n");
    
    printf("System is now running. Timer interrupts will be displayed below:\n");
    printf("(Press Ctrl+A then X to exit QEMU)\n");
    printf("----------------------------------------\n");
    
    while (1) {
        // CPU 在这里等待中断
    }
}