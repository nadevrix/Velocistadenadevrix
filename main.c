/**
 * ============================================================
 *  VELOCISTA - Seguidor de Línea (QTR-8A Analógico)
 *  STM32F401/F411 (BlackPill) Bare-Metal | Driver TB6612FNG
 *  Lógica Simple (Centrado Bang-Bang) | Modo Medio Puente (MacGyver)
 * ============================================================
 */

#include <stdint.h>

/* ============================================================ */
/*    PARÁMETROS DE CALIBRACIÓN (Ajustar aquí)                  */
/* ============================================================ */

/* Velocidades para la lógica simple (0..255) */
#define VELOCIDAD_RECTA 140
#define VELOCIDAD_GIRO_RAPIDO 180
#define VELOCIDAD_GIRO_LENTO 90
#define VELOCIDAD_CERO 0

/* Compensación mecánica de motores */
#define OFFSET_IZQ 0
#define OFFSET_DER 0

/* Umbral ADC para considerar "negro".
 * Con este sensor: Negro = Voltaje BAJO (< UMBRAL), Blanco = Voltaje ALTO (> UMBRAL) */
#define UMBRAL_NEGRO 1500

/* ============================================================ */
/*    REGISTROS STM32F4                                         */
/* ============================================================ */

#define RCC_BASE          0x40023800
#define RCC_AHB1ENR       (*(volatile uint32_t *)(RCC_BASE + 0x30))
#define RCC_APB2ENR       (*(volatile uint32_t *)(RCC_BASE + 0x44))

#define GPIOA_BASE        0x40020000
#define GPIOA_MODER       (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_OSPEEDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x08))
#define GPIOA_PUPDR       (*(volatile uint32_t *)(GPIOA_BASE + 0x0C))
#define GPIOA_AFRH        (*(volatile uint32_t *)(GPIOA_BASE + 0x24))

#define GPIOB_BASE        0x40020400
#define GPIOB_MODER       (*(volatile uint32_t *)(GPIOB_BASE + 0x00))
#define GPIOB_BSRR        (*(volatile uint32_t *)(GPIOB_BASE + 0x18))

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
#define TIM1_CCR2         (*(volatile uint32_t *)(TIM1_BASE + 0x38))
#define TIM1_BDTR         (*(volatile uint32_t *)(TIM1_BASE + 0x44))
#define TIM1_EGR          (*(volatile uint32_t *)(TIM1_BASE + 0x14))

#define ADC1_BASE         0x40012000
#define ADC1_SR           (*(volatile uint32_t *)(ADC1_BASE + 0x00))
#define ADC1_CR1          (*(volatile uint32_t *)(ADC1_BASE + 0x04))
#define ADC1_CR2          (*(volatile uint32_t *)(ADC1_BASE + 0x08))
#define ADC1_SMPR2        (*(volatile uint32_t *)(ADC1_BASE + 0x10))
#define ADC1_SQR1         (*(volatile uint32_t *)(ADC1_BASE + 0x2C))
#define ADC1_SQR3         (*(volatile uint32_t *)(ADC1_BASE + 0x34))
#define ADC1_DR           (*(volatile uint32_t *)(ADC1_BASE + 0x4C))

#define USART6_BASE       0x40011400
#define USART6_SR         (*(volatile uint32_t *)(USART6_BASE + 0x00))
#define USART6_DR         (*(volatile uint32_t *)(USART6_BASE + 0x04))
#define USART6_BRR        (*(volatile uint32_t *)(USART6_BASE + 0x08))
#define USART6_CR1        (*(volatile uint32_t *)(USART6_BASE + 0x0C))

/* ============================================================ */
/*    VARIABLES GLOBALES                                        */
/* ============================================================ */

/* Estado para saber a dónde girar si se pierde la línea */
/* 0 = Centro, 1 = Izquierda, 2 = Derecha */
int ultimo_estado = 0; 

static int tele_count = 0;
uint32_t raw_val[8];
uint8_t sensor_val[8]; /* 1 si detecta línea, 0 si no */

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
/*    UART (TELEMETRÍA)                                         */
/* ============================================================ */

static void uart_init(void) {
    RCC_APB2ENR |= (1 << 5); /* Reloj USART6 */
    
    uint32_t moder = GPIOC_MODER;
    moder &= ~(0x3 << (6 * 2));
    moder |= (0x2 << (6 * 2));
    GPIOC_MODER = moder;
    
    uint32_t afrl = (*(volatile uint32_t *)(GPIOC_BASE + 0x20));
    afrl &= ~(0xF << (6 * 4));
    afrl |= (0x8 << (6 * 4));
    (*(volatile uint32_t *)(GPIOC_BASE + 0x20)) = afrl;
    
    USART6_BRR = 0x8B;
    USART6_CR1 = (1 << 13) | (1 << 3);
}

static void uart_putc(char c) {
    while (!(USART6_SR & (1 << 7)));
    USART6_DR = c;
}

static void uart_print(const char *str) {
    while (*str) uart_putc(*str++);
}

static void uart_print_num(int32_t num) {
    char buf[12];
    int i = 0;
    if (num < 0) { uart_putc('-'); num = -num; }
    if (num == 0) { uart_putc('0'); return; }
    while (num > 0) { buf[i++] = (num % 10) + '0'; num /= 10; }
    while (i > 0)   { uart_putc(buf[--i]); }
}

static void imprimir_telemetria(void) {
    if (++tele_count > 50) {
        tele_count = 0;
        uart_print("BIN:");
        for (int i = 0; i < 8; i++) {
            uart_putc(' ');
            uart_print_num(sensor_val[i]);
        }
        uart_print("\r\n");
    }
}

/* ============================================================ */
/*    HARDWARE INIT (ADC, TIMERS, GPIO)                         */
/* ============================================================ */

static void bsp_init(void) {
    RCC_AHB1ENR |= (1 << 0) | (1 << 1) | (1 << 2);
    RCC_APB2ENR |= (1 << 0) | (1 << 8);

    uint32_t cm = GPIOC_MODER;
    cm &= ~(0x3 << (13 * 2));
    cm |= (0x1 << (13 * 2));
    GPIOC_MODER = cm;
    led_off();

    uint32_t am = GPIOA_MODER;
    am &= ~(0xFFFF);
    am |= 0xFFFF;
    am &= ~(0x3 << (8 * 2)); am |= (0x2 << (8 * 2));
    am &= ~(0x3 << (9 * 2)); am |= (0x2 << (9 * 2));
    GPIOA_MODER = am;

    GPIOA_PUPDR &= ~(0xFFFF);
    GPIOA_OSPEEDR |= (0x3 << (8 * 2)) | (0x3 << (9 * 2));

    uint32_t afrh = GPIOA_AFRH;
    afrh &= ~(0xFF);
    afrh |= 0x11;
    GPIOA_AFRH = afrh;

    uint32_t bm = GPIOB_MODER;
    bm &= ~((0x3 << (6*2)) | (0x3 << (7*2)) | (0x3 << (8*2)) | (0x3 << (9*2)));
    bm |=  ((0x1 << (6*2)) | (0x1 << (7*2)) | (0x1 << (8*2)) | (0x1 << (9*2)));
    GPIOB_MODER = bm;

    TIM1_PSC  = 0;
    TIM1_ARR  = 255;
    TIM1_CCMR1 = (0x6 << 4) | (1 << 3) | (0x6 << 12) | (1 << 11);
    TIM1_CCER  = (1 << 0) | (1 << 4);
    TIM1_BDTR  = (1 << 15);
    TIM1_CCR1  = 0;
    TIM1_CCR2  = 0;
    TIM1_EGR   = 1;
    TIM1_CR1   = (1 << 7) | (1 << 0);

    ADC1_CR2   = 0;
    ADC1_CR1   = 0;
    ADC1_SMPR2 = (0x4 << 0)  | (0x4 << 3)  | (0x4 << 6)  | (0x4 << 9)
               | (0x4 << 12) | (0x4 << 15) | (0x4 << 18) | (0x4 << 21);
    ADC1_SQR1  = 0;
    ADC1_CR2   = (1 << 0);
    delay_ms(10);
    
    uart_init();
}

static uint32_t adc_leer(uint32_t canal) {
    ADC1_SQR3 = canal & 0x1F;
    ADC1_SR = 0;
    ADC1_CR2 |= (1 << 30);
    while (!(ADC1_SR & (1 << 1)));
    return ADC1_DR;
}

/* ============================================================ */
/*    CONTROL DE MOTORES                                        */
/* ============================================================ */

static void set_motores(int32_t speed_izq, int32_t speed_der) {
    /* MODO MACGYVER (Medio Puente A)
     * PWMA siempre al 100% (255).
     * Motores conectados a VMOT (positivo) y a AO1/AO2 (negativo).
     */
    TIM1_CCR1 = 255; 
    
    int izq_on = (speed_izq > 0) ? 1 : 0;
    int der_on = (speed_der > 0) ? 1 : 0;

    if (izq_on && der_on) {
        /* IN1=HIGH, IN2=HIGH -> AO1=LOW, AO2=LOW (Ambos Motores ON) */
        GPIOB_BSRR = (1 << 8) | (1 << 9);
    } 
    else if (izq_on && !der_on) {
        /* IN1=LOW, IN2=HIGH -> AO1=LOW, AO2=HIGH (Solo Izquierdo ON) */
        GPIOB_BSRR = (1 << (8 + 16)) | (1 << 9);
    } 
    else if (der_on && !izq_on) {
        /* IN1=HIGH, IN2=LOW -> AO1=HIGH, AO2=LOW (Solo Derecho ON) */
        GPIOB_BSRR = (1 << 8) | (1 << (9 + 16));
    } 
    else {
        /* IN1=LOW, IN2=LOW -> AO1=OFF, AO2=OFF (Ambos Motores OFF) */
        GPIOB_BSRR = (1 << (8 + 16)) | (1 << (9 + 16));
    }
}

/* ============================================================ */
/*    LÓGICA DEL SEGUIDOR (IF/ELSE)                             */
/* ============================================================ */

static int leer_sensores(void) {
    int linea_detectada = 0;

    for (int i = 0; i < 8; i++) {
        raw_val[i] = adc_leer(i);

        if (raw_val[i] > UMBRAL_NEGRO) {
            sensor_val[i] = 1;
            linea_detectada = 1;
        } else {
            sensor_val[i] = 0;
        }
    }
    return linea_detectada;
}

/* ============================================================ */
/*    PROGRAMA PRINCIPAL                                        */
/* ============================================================ */

int main(void) {
    bsp_init();
    uart_print("\r\n--- VELOCISTA ENCENDIDO (LOGICA SIMPLE) ---\r\n");

    /* Espera 3 segundos para colocar el carro */
    for (int i = 0; i < 3; i++) {
        set_motores(0, 0);
        led_on();  delay_ms(100);
        led_off(); delay_ms(900);
    }

    /* Bucle Principal */
    while (1) {
        int linea_detectada = leer_sensores();
        imprimir_telemetria();

        if (linea_detectada) {
            led_on();

            /* LÓGICA DE CENTRADO (BANG-BANG)
             * El objetivo es mantener SIEMPRE el negro en el centro (S3, S4).
             * Si el negro toca las esquinas (que deberían ser blancas), corregimos bruscamente.
             */
            
            /* Izquierda (S0, S1, S2) ven negro -> El carro se desvió a la derecha.
             * Hay que girar a la izquierda. Como "se ahuyentaba", invertimos la orden
             * para compensar tus cables cruzados:
             */
            if (sensor_val[0] || sensor_val[1] || sensor_val[2]) {
                set_motores(VELOCIDAD_RECTA, VELOCIDAD_CERO); /* Avanza Izq, Frena Der (Invertido) */
                ultimo_estado = 1;
            }
            /* Derecha (S5, S6, S7) ven negro -> El carro se desvió a la izquierda.
             * Hay que girar a la derecha. 
             */
            else if (sensor_val[7] || sensor_val[6] || sensor_val[5]) {
                set_motores(VELOCIDAD_CERO, VELOCIDAD_RECTA); /* Frena Izq, Avanza Der (Invertido) */
                ultimo_estado = 2;
            }
            /* Centro (S3, S4) -> ¡Todo perfecto! Avanzar Recto */
            else if (sensor_val[3] || sensor_val[4]) {
                set_motores(VELOCIDAD_RECTA, VELOCIDAD_RECTA);
                ultimo_estado = 0;
            }
        } 
        else {
            /* Perdió la línea por completo: girar hacia el último lado conocido */
            led_off();
            
            if (ultimo_estado == 1) {
                set_motores(VELOCIDAD_RECTA, VELOCIDAD_CERO); /* Búsqueda Izquierda invertida */
            } else if (ultimo_estado == 2) {
                set_motores(VELOCIDAD_CERO, VELOCIDAD_RECTA); /* Búsqueda Derecha invertida */
            } else {
                set_motores(VELOCIDAD_RECTA, VELOCIDAD_RECTA);
            }
        }
        
        delay_ms(1);
    }

    return 0;
}
