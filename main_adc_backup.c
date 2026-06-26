"/**
 * ============================================================
 *  VELOCISTA - Robot Seguidor de LÃ­nea (QTR-8A AnalÃ³gico)
 *  STM32F401/F411 (BlackPill) Bare-Metal | Driver TB6612FNG
 * ============================================================
 *
 * SENSOR: Pololu QTR-8A (ANALÃGICO, sin comparador)
 *   - Salida = voltaje variable segÃºn reflectancia
 *   - Negro (lÃ­nea):  poca reflexiÃ³n â voltaje ALTO (~3000-4095)
 *   - Blanco (fondo): mucha reflexiÃ³n â voltaje BAJO (~0-500)
 *   - Se lee con ADC de 12 bits (0 a 4095)
 *   - Umbral por software configurable
 *
 * Pines:
 *   Sensores QTR-8A: PA0..PA7 â ADC1 canales 0-7
 *   Motor A (Izq):   PWMA=PA8(TIM1_CH1), AIN1=PB8, AIN2=PB9
 *   Motor B (Der):   PWMB=PA9(TIM1_CH2), BIN1=PB6, BIN2=PB7
 *   LED diagnÃ³stico: PC13 (active LOW)
 *   STBY driver:     3V3
 *
 *   QTR-8A VCC â 3.3V o 5V
 *   QTR-8A GND â GND
 *   QTR-8A pins 1-8 â PA0-PA7
 */

#include <stdint.h>

/* ============================================================ */
/*    REGISTROS STM32F401/F411                                  */
/* ============================================================ */

/* RCC */
#define RCC_BASE          0x40023800
#define RCC_AHB1ENR       (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR       (*(volatile uint32_t *)(RCC_BASE + 0x44))

/* GPIOA */
#define GPIOA_BASE        0x40020000
#define GPIOA_MODER       (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_OSPEEDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x08))
#define GPIOA_PUPDR       (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))
#define GPIOA_AFRH        (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

/* GPIOB */
#define GPIOB_BASE        0x40020400
#define GPIOB_MODER       (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_OSPEEDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x08))
#define GPIOB_BSRR        (*(volatile uint32_t *)(GPIOB_BASE + 0x18))

/* GPIOC */
#define GPIOC_BASE        0x40020800
#define GPIOC_MOD
<truncated 10923 bytes>
