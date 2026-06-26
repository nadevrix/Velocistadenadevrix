# Anexo A: Lógica Discreta (Bang-Bang) para STM32F4 BlackPill
El presente documento contiene la implementación del algoritmo de control discreto (Bang-Bang por zonas) desarrollado en C puro para el microcontrolador STM32F401 / STM32F411 (BlackPill).

Este código incluye la modificación topológica de hardware ("Low-Side Switching" o "MacGyver Hack") en el puente H TB6612FNG, permitiendo el control independiente de dos motores DC utilizando únicamente el Canal A.

```c
/**
 * ============================================================
 *  VELOCISTA - Seguidor de Línea (QTR-8A Analógico)
 *  STM32F401/F411 (BlackPill) Bare-Metal | Driver TB6612FNG
 *  Lógica Simple (Centrado Bang-Bang) | Modo Medio Puente (MacGyver)
 * ============================================================
 */

#include <stdint.h>

#define VELOCIDAD_RECTA 140
#define VELOCIDAD_CERO 0
#define UMBRAL_NEGRO 1500

/* ... [Registros de Hardware omitidos por brevedad, son los mismos] ... */

static void set_motores(int32_t speed_izq, int32_t speed_der) {
    /* MODO MACGYVER (Medio Puente A)
     * PWMA siempre al 100% (255).
     * Motores conectados a VMOT (positivo) y a AO1/AO2 (negativo).
     */
    TIM1_CCR1 = 255; 
    
    int izq_on = (speed_izq > 0) ? 1 : 0;
    int der_on = (speed_der > 0) ? 1 : 0;

    if (izq_on && der_on) {
        GPIOB_BSRR = (1 << 8) | (1 << 9);
    } 
    else if (izq_on && !der_on) {
        GPIOB_BSRR = (1 << (8 + 16)) | (1 << 9);
    } 
    else if (der_on && !izq_on) {
        GPIOB_BSRR = (1 << 8) | (1 << (9 + 16));
    } 
    else {
        GPIOB_BSRR = (1 << (8 + 16)) | (1 << (9 + 16));
    }
}

/* ... [Bucle Principal] ... */
    while (1) {
        int linea_detectada = leer_sensores();
        if (linea_detectada) {
            if (sensor_val[0] || sensor_val[1] || sensor_val[2]) {
                set_motores(VELOCIDAD_RECTA, VELOCIDAD_CERO); /* Avanza Izq, Frena Der (Invertido) */
                ultimo_estado = 1;
            }
            else if (sensor_val[7] || sensor_val[6] || sensor_val[5]) {
                set_motores(VELOCIDAD_CERO, VELOCIDAD_RECTA); /* Frena Izq, Avanza Der (Invertido) */
                ultimo_estado = 2;
            }
            else if (sensor_val[3] || sensor_val[4]) {
                set_motores(VELOCIDAD_RECTA, VELOCIDAD_RECTA);
                ultimo_estado = 0;
            }
        } else {
            if (ultimo_estado == 1) set_motores(VELOCIDAD_RECTA, VELOCIDAD_CERO);
            else if (ultimo_estado == 2) set_motores(VELOCIDAD_CERO, VELOCIDAD_RECTA);
            else set_motores(VELOCIDAD_RECTA, VELOCIDAD_RECTA);
        }
    }
```
