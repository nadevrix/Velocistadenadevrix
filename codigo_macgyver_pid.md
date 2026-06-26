# Código Velocista "MacGyver" (PID Completo)
Este código utiliza el Timer 4 (TIM4) para enviar señales PWM independientes a los motores a pesar de estar conectados al único canal que sobrevivió en el driver TB6612FNG.
Te permite usar el algoritmo PID clásico y realizar correcciones de giro suaves.

```c
/**
 * ============================================================
 *  VELOCISTA - Seguidor de Línea (QTR-8A Analógico)
 *  STM32F401/F411 (BlackPill) Bare-Metal | Driver TB6612FNG
 *  Lógica PID Completa | Modo Medio Puente (MacGyver + TIM4 PWM)
 * ============================================================
 */

#include <stdint.h>

/* ============================================================ */
/*    PARÁMETROS DE CALIBRACIÓN (Ajustar aquí)                  */
/* ============================================================ */

/* Velocidades para el PID (0..255) */
#define VELOCIDAD_BASE 150
#define VELOCIDAD_MAX  255

/* Constantes PID 
 * Empieza subiendo solo KP hasta que oscile.
 * Luego sube KD para frenar la oscilación. */
#define KP 0.05f
#define KD 0.3f
#define KI 0.0f

#define UMBRAL_NEGRO 1500

/* ============================================================ */
/*    REGISTROS STM32F4                                         */
/* ============================================================ */

#define RCC_BASE          0x40023800
#define RCC_AHB1ENR       (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB1ENR       (*(volatile uint32_t *)(RCC_BASE + 0x40))
#define RCC_APB2ENR       (*(volatile uint32_t *)(RCC_BASE + 0x44))

#define GPIOA_BASE        0x40020000
#define GPIOA_MODER       (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_OSPEEDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x08))
#define GPIOA_PUPDR       (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))
#define GPIOA_AFRH        (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

#define GPIOB_BASE        0x40020400
#define GPIOB_MODER       (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_AFRH        (*(volatile uint32_t *)(GPIOB_BASE + 0x24))

#define GPIOC_BASE        0x40020800
#define GPIOC_MODER       (*(volatile uint32_t *)(GPIOC_BASE + 0x00))
#define GPIOC_BSRR        (*(volatile uint32_t *)(GPIOC_BASE + 0x18))

#define TIM1_BASE         0x40010000
#define TIM1_CR1          (*(volatile uint32_t *)(TIM1_BASE + 0x00))
#define TIM1_CCMR1        (*(volatile uint32_t *)(TIM1_BASE + 0x18))
#define TIM1_CCER         (*(volatile uint32_t *)(TIM1_BASE + 0x20))
#define TIM1_PSC          (*(volatile uint32_t *)(TIM1_BASE + 0x28))
#define TIM1_ARR          (*(volatile uint32_t *)(TIM1_BASE + 0x2C))
#define TIM1_CCR1         (*(volatile uint32_t *)(TIM1_BASE + 0x34))
#define TIM1_BDTR         (*(volatile uint32_t *)(TIM1_BASE + 0x44))
#define TIM1_EGR          (*(volatile uint32_t *)(TIM1_BASE + 0x14))

#define TIM4_BASE         0x40000800
#define TIM4_CR1          (*(volatile uint32_t *)(TIM4_BASE + 0x00))
#define TIM4_CCMR2        (*(volatile uint32_t *)(TIM4_BASE + 0x1C))
#define TIM4_CCER         (*(volatile uint32_t *)(TIM4_BASE + 0x20))
#define TIM4_PSC          (*(volatile uint32_t *)(TIM4_BASE + 0x28))
#define TIM4_ARR          (*(volatile uint32_t *)(TIM4_BASE + 0x2C))
#define TIM4_CCR3         (*(volatile uint32_t *)(TIM4_BASE + 0x3C))
#define TIM4_CCR4         (*(volatile uint32_t *)(TIM4_BASE + 0x40))
#define TIM4_EGR          (*(volatile uint32_t *)(TIM4_BASE + 0x14))

#define ADC1_BASE         0x40012000
#define ADC1_SR           (*(volatile uint32_t *)(ADC1_BASE + 0x00))
#define ADC1_CR1          (*(volatile uint32_t *)(ADC1_BASE + 0x04))
#define ADC1_CR2          (*(volatile uint32_t *)(ADC1_BASE + 0x08))
#define ADC1_SMPR2        (*(volatile uint32_t *)(ADC1_BASE + 0x10))
#define ADC1_SQR1         (*(volatile uint32_t *)(ADC1_BASE + 0x2C))
#define ADC1_SQR3         (*(volatile uint32_t *)(ADC1_BASE + 0x34))
#define ADC1_DR           (*(volatile uint32_t *)(ADC1_BASE + 0x4C))

/* ============================================================ */
/*    VARIABLES GLOBALES                                        */
/* ============================================================ */

uint32_t raw_val[8];
int linea_detectada = 0;
int ultima_posicion = 3500;
float error_previo = 0;
float error_acumulado = 0;

/* ============================================================ */
/*    FUNCIONES AUXILIARES                                      */
/* ============================================================ */

static void delay_ms(volatile uint32_t ms) {
    while (ms--) {
        for (volatile int i = 0; i < 4000; i++) {
            __asm__ volatile ("nop");
        }
    }
}

static inline void led_on(void)  { GPIOC_BSRR = (1 << (13 + 16)); }
static inline void led_off(void) { GPIOC_BSRR = (1 << 13); }

/* ============================================================ */
/*    HARDWARE INIT (ADC, TIMERS, GPIO)                         */
/* ============================================================ */

static void bsp_init(void) {
    RCC_AHB1ENR |= (1 << 0) | (1 << 1) | (1 << 2);
    RCC_APB1ENR |= (1 << 2);  // TIM4
    RCC_APB2ENR |= (1 << 0) | (1 << 8); // ADC, TIM1

    uint32_t cm = GPIOC_MODER;
    cm &= ~(0x3 << (13 * 2));
    cm |= (0x1 << (13 * 2));
    GPIOC_MODER = cm;
    led_off();

    uint32_t am = GPIOA_MODER;
    am &= ~(0xFFFF);
    am |= 0xFFFF; // PA0..PA7 Analógicos
    am &= ~(0x3 << (8 * 2)); am |= (0x2 << (8 * 2)); // PA8 AF (TIM1_CH1)
    GPIOA_MODER = am;

    GPIOA_PUPDR &= ~(0xFFFF);
    GPIOA_OSPEEDR |= (0x3 << (8 * 2));

    uint32_t afrh = GPIOA_AFRH;
    afrh &= ~(0xF);
    afrh |= 0x1; // AF1 para PA8
    GPIOA_AFRH = afrh;

    /* PB8 y PB9 como AF2 para TIM4 (Control PWM MacGyver) */
    uint32_t bm = GPIOB_MODER;
    bm &= ~((0x3 << (8*2)) | (0x3 << (9*2)));
    bm |=  ((0x2 << (8*2)) | (0x2 << (9*2)));
    GPIOB_MODER = bm;

    uint32_t b_afrh = GPIOB_AFRH;
    b_afrh &= ~((0xF << 0) | (0xF << 4));
    b_afrh |=  ((0x2 << 0) | (0x2 << 4)); // AF2
    GPIOB_AFRH = b_afrh;

    /* TIM1 (Genera señal máxima constante para el driver) */
    TIM1_PSC  = 0;
    TIM1_ARR  = 255;
    TIM1_CCMR1 = (0x6 << 4) | (1 << 3);
    TIM1_CCER  = (1 << 0);
    TIM1_BDTR  = (1 << 15);
    TIM1_CCR1  = 255; 
    TIM1_EGR   = 1;
    TIM1_CR1   = (1 << 7) | (1 << 0);

    /* TIM4 (Hardware PWM independiente para Motor Izq y Der) */
    TIM4_PSC  = 0;
    TIM4_ARR  = 255;
    TIM4_CCMR2 = (0x6 << 4) | (1 << 3) | (0x6 << 12) | (1 << 11);
    TIM4_CCER  = (1 << 8) | (1 << 12);
    TIM4_CCR3  = 0;
    TIM4_CCR4  = 0;
    TIM4_EGR   = 1;
    TIM4_CR1   = (1 << 7) | (1 << 0);

    /* ADC Init */
    ADC1_CR2   = 0;
    ADC1_CR1   = 0;
    ADC1_SMPR2 = (0x4 << 0)  | (0x4 << 3)  | (0x4 << 6)  | (0x4 << 9)
               | (0x4 << 12) | (0x4 << 15) | (0x4 << 18) | (0x4 << 21);
    ADC1_SQR1  = 0;
    ADC1_CR2   = (1 << 0);
    delay_ms(10);
}

static uint32_t adc_leer(uint32_t canal) {
    ADC1_SQR3 = canal & 0x1F;
    ADC1_SR = 0;
    ADC1_CR2 |= (1 << 30);
    while (!(ADC1_SR & (1 << 1)));
    return ADC1_DR;
}

/* ============================================================ */
/*    CONTROL DE MOTORES (MACGYVER + TIM4 PWM)                  */
/* ============================================================ */

static void set_motores(int32_t speed_izq, int32_t speed_der) {
    if (speed_izq < 0) speed_izq = 0;
    if (speed_der < 0) speed_der = 0;
    if (speed_izq > 255) speed_izq = 255;
    if (speed_der > 255) speed_der = 255;

    TIM4_CCR3 = speed_izq; /* PB8 = Motor Izquierdo */
    TIM4_CCR4 = speed_der; /* PB9 = Motor Derecho */
}

/* ============================================================ */
/*    LÓGICA DEL SEGUIDOR (PID)                                 */
/* ============================================================ */

static int leer_posicion(void) {
    uint32_t suma_ponderada = 0;
    uint32_t suma_total = 0;
    linea_detectada = 0;

    for (int i = 0; i < 8; i++) {
        raw_val[i] = adc_leer(i);
        
        uint32_t val = 0;
        if (raw_val[i] > UMBRAL_NEGRO) {
            val = 1000;
            linea_detectada = 1;
        }

        suma_ponderada += val * (i * 1000);
        suma_total += val;
    }

    /* Si se pierde la línea, devolvemos un extremo basado en dónde la vimos por última vez */
    if (!linea_detectada) {
        if (ultima_posicion < 3500) return 0;
        else return 7000;
    }

    ultima_posicion = suma_ponderada / suma_total;
    return ultima_posicion;
}

/* ============================================================ */
/*    PROGRAMA PRINCIPAL                                        */
/* ============================================================ */

int main(void) {
    bsp_init();

    for (int i = 0; i < 3; i++) {
        set_motores(0, 0);
        led_on();  delay_ms(100);
        led_off(); delay_ms(900);
    }

    while (1) {
        int posicion = leer_posicion();
        
        /* El centro ideal es 3500 */
        float error = 3500.0f - (float)posicion;
        
        float correccion = (error * KP) + ((error - error_previo) * KD) + (error_acumulado * KI);
        error_previo = error;
        error_acumulado += error;

        /* Aplicar corrección a la velocidad base */
        int32_t m_izq = VELOCIDAD_BASE - (int32_t)correccion;
        int32_t m_der = VELOCIDAD_BASE + (int32_t)correccion;

        if (!linea_detectada) {
            /* Modo búsqueda brusca para no perder la pista */
            led_off();
            if (ultima_posicion < 3500) {
                set_motores(0, VELOCIDAD_MAX); // Búsqueda Izquierda
            } else {
                set_motores(VELOCIDAD_MAX, 0); // Búsqueda Derecha
            }
        } else {
            /* Seguimiento normal PID */
            led_on();
            set_motores(m_izq, m_der);
        }
        
        delay_ms(1);
    }

    return 0;
}
```
