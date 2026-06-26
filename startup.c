/**
 * startup.c - Startup code for STM32F401/F411 (Cortex-M4)
 * 
 * Contains:
 *   - Vector table (interrupt vectors)
 *   - Reset handler (copies .data, zeros .bss, calls main)
 *   - Default fault handlers
 */

#include <stdint.h>

/* Symbols defined in linker script */
extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

/* Prototype for main */
extern int main(void);

/* Reset handler - entry point after reset */
void Reset_Handler(void) {
    uint32_t *src, *dst;

    /* Copy .data section from Flash to SRAM */
    src = &_sidata;
    dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* Zero .bss section in SRAM */
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }

    /* Enable FPU (Cortex-M4 has FPU) */
    /* CPACR: set CP10 and CP11 to full access */
    *(volatile uint32_t *)0xE000ED88 |= (0xF << 20);
    __asm__ volatile ("dsb");
    __asm__ volatile ("isb");

    /* Call main */
    main();

    /* If main returns, loop forever */
    while (1);
}

/* Default handler for unimplemented interrupts */
void Default_Handler(void) {
    while (1);
}

/* Weak aliases - can be overridden by user code */
void NMI_Handler(void)         __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)         __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)      __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)     __attribute__((weak, alias("Default_Handler")));

/* ============================================ */
/*    VECTOR TABLE (STM32F401/F411)             */
/* ============================================ */
__attribute__((section(".isr_vector")))
uint32_t *isr_vectors[] = {
    (uint32_t *)&_estack,           /* 0x0000: Initial Stack Pointer */
    (uint32_t *)Reset_Handler,      /* 0x0004: Reset */
    (uint32_t *)NMI_Handler,        /* 0x0008: NMI */
    (uint32_t *)HardFault_Handler,  /* 0x000C: Hard Fault */
    (uint32_t *)MemManage_Handler,  /* 0x0010: Memory Management Fault */
    (uint32_t *)BusFault_Handler,   /* 0x0014: Bus Fault */
    (uint32_t *)UsageFault_Handler, /* 0x0018: Usage Fault */
    0, 0, 0, 0,                     /* 0x001C-0x0028: Reserved */
    (uint32_t *)SVC_Handler,        /* 0x002C: SVCall */
    (uint32_t *)DebugMon_Handler,   /* 0x0030: Debug Monitor */
    0,                              /* 0x0034: Reserved */
    (uint32_t *)PendSV_Handler,     /* 0x0038: PendSV */
    (uint32_t *)SysTick_Handler,    /* 0x003C: SysTick */
    /* IRQ 0-84 for STM32F401/F411 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* IRQ 0-9   */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* IRQ 10-19 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* IRQ 20-29 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* IRQ 30-39 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* IRQ 40-49 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* IRQ 50-59 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* IRQ 60-69 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* IRQ 70-79 */
    0, 0, 0, 0, 0,                  /* IRQ 80-84 */
};
