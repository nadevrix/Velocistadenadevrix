# INFORME TÉCNICO
**Robot Seguidor de Línea Velocista**  
*Programación Bare-Metal en C • STM32F401/F411 • Bang-Bang y PD/PID*

**Asignatura:** Sistemas Embebidos / Electrónica  
**Microcontrolador:** STM32F401 / STM32F411 BlackPill (Bare-Metal C)  
**Autor:** Rodrigo Ricaldez Martínez  
**Fecha:** Junio 2026  

---

## 1. Introducción y Objetivo
El presente proyecto tiene como objetivo el diseño, ensamblaje y programación de un robot seguidor de línea (velocista) de alto rendimiento. Para lograr tiempos de respuesta críticos y control absoluto sobre los periféricos, se descartó el uso de librerías de abstracción de alto nivel (como Arduino o HAL) y se implementó una arquitectura de programación Bare-Metal en lenguaje C puro sobre un microcontrolador de la familia ARM Cortex-M4.

La filosofía Bare-Metal implica manipular directamente los registros físicos del microcontrolador (RCC, GPIO, ADC1, TIM1, TIM4) apuntando a sus direcciones de memoria hexadecimales, lo que reduce la latencia de procesamiento casi a cero y garantiza ciclos de control deterministas. Se desarrollaron y compararon dos algoritmos de control distintos: un control discreto Bang-Bang por zonas (Fase 1) y un control continuo Proporcional-Derivativo (PD/PID) con PWM de hardware (Fase 2).

## 2. Hardware Utilizado (Lista de Materiales)

| Componente | Modelo / Especificación | Función en el sistema |
| :--- | :--- | :--- |
| **Microcontrolador** | STM32F401/F411 BlackPill | Cerebro del robot. ARM Cortex-M4 a 84/100 MHz, ADC 12 bits multicanal, timers de hardware para PWM. |
| **Driver de potencia** | Toshiba TB6612FNG (Puente H dual MOSFET) | Controla velocidad y giro de motores. Régimen ~0.5 Ω vs ~1 Ω del L298N; menor caída de tensión y mayor eficiencia. |
| **Barra de sensores** | Pololu QTR-8A (8 canales analógicos IR) | Detecta la posición lateral de la línea por reflectancia infrarroja. Salida analógica en voltaje, leída por ADC1. |
| **Motores** | 2x Motorreductor Micro Metal N20 | Actuadores de tracción. Alta relación torque/tamaño, ideales para chasis compacto velocista. |
| **Ruedas** | Ruedas con llanta de goma | Proporcionan adherencia y agarre sobre la superficie de la pista, reduciendo el deslizamiento en curvas. |
| **Chasis** | Chasis compacto para robot velocista | Estructura mecánica que aloja todos los componentes; diseñado para minimizar peso y centrar la masa. |
| **Alimentación** | 4x Pilas AA Panasonic en serie (6 V nom.) | Fuente de energía accesible y estándar para la etapa de potencia. La STM32 regula 3.3 V para su lógica interna. |

### 2.1 Fuente de Alimentación: 4 Pilas AA en Serie
Se utilizó un portapilas con 4 pilas AA Panasonic conectadas en serie como fuente de alimentación para la etapa de potencia. Esta configuración entrega un voltaje nominal de 6 V (1.5 V por celda × 4), compatible con el rango de `VMOT` del TB6612FNG (2.5 V a 15 V máx.) y suficiente para operar los motorreductores N20 dentro de su rango nominal.

> **Nota:** Ventajas de las pilas AA: fácil reemplazo en competencia, bajo riesgo de cortocircuito frente a LiPo, disponibilidad inmediata y costo reducido. La principal desventaja es la caída de tensión a medida que se descargan, lo que puede afectar la velocidad máxima al final de la carga.

## 3. Arquitectura Eléctrica y Topología Low-Side Switch
Durante la fase de integración se detectó una falla de hardware en el Canal B del driver TB6612FNG. Para solventar este contratiempo sin reemplazar el circuito integrado, se diseñó e implementó una topología eléctrica alternativa aprovechando únicamente el Canal A operativo.

Dado que el algoritmo del velocista fue diseñado para velocidades netamente positivas (sin inversión de giro), el puente H se configuró para operar como un doble interruptor de lado bajo (*Low-Side Switch*):

* **Terminal positivo de ambos motores:** conectado directamente a `VMOT` (positivo de las pilas, 6 V).
* **Terminal negativo Motor Izquierdo:** conectado a la salida `AO1` del driver.
* **Terminal negativo Motor Derecho:** conectado a la salida `AO2` del driver.

Al saturar la señal principal (`PWMA`) al 100% por software, los pines `AIN1` y `AIN2` controlan de forma directa el cierre del circuito hacia GND de cada motor de forma totalmente independiente, a pesar de compartir el mismo canal físico del driver.

> **Fundamento técnico:** cuando `AIN1 = HIGH`, la salida `AO1` se conecta internamente a `GND`, creando el camino *VMOT → Motor Der. → AO1 → GND*. El mismo principio aplica para `AIN2`/`AO2`. Se sacrifica únicamente la capacidad de marcha atrás, irrelevante para un velocista.

### 3.1 Mapa de Pines (Pinout STM32)

| Pin STM32 | Periférico / Señal | Descripción |
| :--- | :--- | :--- |
| **PA0 – PA7** | ADC1-IN0 a IN7 | Entradas analógicas de los 8 sensores QTR-8A (S1 a S8) |
| **PA8** | TIM1_CH1 (PWMA) | Señal PWM al driver; 100% en Bang-Bang, modulada en PD/PID |
| **PB8** | AIN1 / TIM4_CH3 | Activa Motor Derecho (digital en Fase 1; PWM hardware en Fase 2) |
| **PB9** | AIN2 / TIM4_CH4 | Activa Motor Izquierdo (digital en Fase 1; PWM hardware en Fase 2) |
| **3.3 V** | STBY (driver) | Mantiene el TB6612FNG siempre activo (nivel HIGH permanente) |
| **VMOT (pilas)** | Motores (+) | Alimentación directa de los terminales positivos de ambos motores |

## 4. Entorno de Desarrollo, Herramientas y Plantilla (Software)
Para maximizar el rendimiento y aprender la arquitectura a bajo nivel, no se utilizaron IDEs comerciales (como STM32CubeIDE) ni plataformas de hobbistas (como Arduino IDE). El proyecto se desarrolló desde cero bajo el siguiente ecosistema:

* **Sistema Operativo:** Arch Linux con gestor de ventanas i3wm y emulador de terminal Kitty.
* **Lenguaje de Programación:** C puro (estándar C99). Elegido por su eficiencia extrema, control directo sobre la memoria y capacidad de manipulación de bits a bajo nivel.
* **Toolchain (Compilador):** GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`). Compilador estándar de la industria para microcontroladores ARM.
* **Sistema de Construcción (Makefile):** Se diseñó una plantilla personalizada gestionada enteramente a través de un archivo `Makefile`, que automatiza el proceso de compilación, enlazado y generación de los binarios `.elf` y `.bin`.
* **Arranque y Memoria (Startup & Linker):** Se programó un archivo de arranque personalizado (`startup.c`) para inicializar los vectores de interrupción, la sección `.bss` y la sección de datos, apoyado por un archivo enlazador (`stm32f4.ld`) para organizar la memoria Flash y RAM.
* **Flasheo:** La carga del código al microcontrolador se realiza por línea de comandos utilizando `dfu-util` o `st-flash` (`sudo make flash`).
* **Paradigma Bare-Metal:** Se manipularon directamente los registros físicos del microcontrolador (RCC, GPIO, ADC1, TIM1, TIM4) apuntando a sus direcciones de memoria hexadecimales. Al prescindir de librerías HAL o de abstracción, el binario resultante es sumamente ligero (< 2 KB) y la latencia de ejecución es casi cero, un factor crítico para un velocista.

### 4.1 Estructura del Proyecto
```text
seguidor-linea/
├── Makefile          # Reglas de compilacion, enlazado y flash
├── startup.c         # Tabla de vectores, inicializacion .bss y .data
├── stm32f4.ld        # Linker script: mapeo de Flash (512KB) y RAM (96KB)
├── src/
│   ├── main.c         # Logica principal + algoritmo de control
│   ├── bsp.c          # Board Support: init RCC, GPIO, ADC1, TIM1, TIM4
│   └── bsp.h          # Definicion de registros por direccion de memoria
└── build/
    ├── firmware.elf   # Binario con simbolos de depuracion
    └── firmware.bin   # Binario plano para flashear (<2 KB)
```

### 4.2 Flujo de Trabajo

| Comando | Acción |
| :--- | :--- |
| `make` | Compila todo el proyecto con arm-none-eabi-gcc y genera .elf y .bin |
| `make flash` | Carga el firmware al STM32 vía DFU (USB) con dfu-util o via SWD con st-flash |
| `make clean` | Elimina los archivos generados en build/ |

## 5. Algoritmos de Control Desarrollados
Con el objetivo de evaluar la respuesta del chasis ante distintas estrategias, se implementaron dos lógicas de control bien diferenciadas:

### 5.1 Fase 1: Control Discreto Bang-Bang por Zonas
Una máquina de estados que determina la acción del robot únicamente por las zonas de la barra QTR-8A que detectan la línea. PWMA se mantiene saturado al 100% y AIN1/AIN2 operan como interruptores digitales puros via `GPIOB_BSRR`:

| Condición (sensores activos) | Acción | Descripción |
| :--- | :--- | :--- |
| **S0, S1, S2** (zona izquierda) | Motor Izq. FULL – Motor Der. STOP | Curva cerrada a la derecha: pivote agresivo re-centra la línea |
| **S3, S4** (zona central) | Ambos motores FULL | Línea recta: máxima velocidad |
| **S5, S6, S7** (zona derecha) | Motor Der. FULL – Motor Izq. STOP | Curva cerrada a la izquierda: pivote agresivo |
| **Ninguno** (línea perdida) | Consulta último estado válido | Barrido hacia el último lado donde se vio la línea |

* **Ventaja:** lógica infalible y fácil de depurar; no requiere calibración de parámetros.
* **Limitación:** el comportamiento ON/OFF produce oscilaciones (rebotes) en tramos rectos a alta velocidad.

### 5.2 Fase 2: Control Proporcional-Derivativo (PD/PID)
Una vez estabilizado el hardware, se configuró el Timer 4 (TIM4) para generar señales PWM independientes por hardware en PB8 (TIM4_CH3) y PB9 (TIM4_CH4), permitiendo velocidades continuas por motor. El algoritmo calcula una posición ponderada de la línea en el rango [0, 7000] (centro = 3500):

`corrección = (error × Kp) + ((error − error_previo) × Kd) + (error_acumulado × Ki)`

| Parámetro | Valor | Descripción |
| :--- | :--- | :--- |
| **Kp** | 0.05 | Proporcional: corrige el error actual de posición. |
| **Kd** | 0.3 | Derivativo: anticipa el cambio del error, amortigua oscilaciones. |
| **Ki** | 0.0 | Integral: mantenido en 0; no hubo error de estado estacionario en la pista utilizada. |
| **VELOCIDAD_BASE** | 150 | Velocidad de crucero (0–255 duty cycle PWM TIM4). |

## 6. Código Fuente

### 6.1 Fase 1: Lógica Bang-Bang
```c
#include <stdint.h>
#define VELOCIDAD_RECTA  140
#define VELOCIDAD_CERO     0
#define UMBRAL_NEGRO    1500
 
static void set_motores(int32_t speed_izq, int32_t speed_der) {
    TIM1_CCR1 = 255;  /* PWMA al 100%: driver siempre habilitado */
 
    int izq_on = (speed_izq > 0) ? 1 : 0;
    int der_on = (speed_der > 0) ? 1 : 0;
 
    if      ( izq_on &&  der_on) GPIOB_BSRR = (1<<8)|(1<<9);
    else if ( izq_on && !der_on) GPIOB_BSRR = (1<<(8+16))|(1<<9);
    else if (!izq_on &&  der_on) GPIOB_BSRR = (1<<8)|(1<<(9+16));
    else                         GPIOB_BSRR = (1<<(8+16))|(1<<(9+16));
}
 
int main(void) {
    bsp_init();
    while (1) {
        int linea_detectada = leer_sensores();
        if (linea_detectada) {
            if (sensor_val[0]||sensor_val[1]||sensor_val[2]) {
                set_motores(VELOCIDAD_RECTA, VELOCIDAD_CERO); ultimo_estado = 1;
            } else if (sensor_val[7]||sensor_val[6]||sensor_val[5]) {
                set_motores(VELOCIDAD_CERO, VELOCIDAD_RECTA); ultimo_estado = 2;
            } else if (sensor_val[3]||sensor_val[4]) {
                set_motores(VELOCIDAD_RECTA, VELOCIDAD_RECTA); ultimo_estado = 0;
            }
        } else {
            if (ultimo_estado == 1) set_motores(VELOCIDAD_RECTA, VELOCIDAD_CERO);
            else if (ultimo_estado == 2) set_motores(VELOCIDAD_CERO, VELOCIDAD_RECTA);
            else set_motores(VELOCIDAD_RECTA, VELOCIDAD_RECTA);
        }
    }
}
```

### 6.2 Fase 2: Lógica PD/PID con PWM de Hardware (TIM4)
```c
#include <stdint.h>
#define VELOCIDAD_BASE  150
#define VELOCIDAD_MAX   255
#define KP              0.05f
#define KD              0.3f
#define KI              0.0f
#define UMBRAL_NEGRO   1500
 
static void set_motores(int32_t speed_izq, int32_t speed_der) {
    if (speed_izq < 0)   speed_izq = 0;
    if (speed_der < 0)   speed_der = 0;
    if (speed_izq > 255) speed_izq = 255;
    if (speed_der > 255) speed_der = 255;
    TIM4_CCR3 = speed_izq;  /* PB8 = Motor Izquierdo */
    TIM4_CCR4 = speed_der;  /* PB9 = Motor Derecho   */
}
 
static int leer_posicion(void) {
    uint32_t suma_ponderada = 0, suma_total = 0;
    linea_detectada = 0;
    for (int i = 0; i < 8; i++) {
        raw_val[i] = adc_leer(i);
        uint32_t val = (raw_val[i] > UMBRAL_NEGRO) ? 1000 : 0;
        if (val) linea_detectada = 1;
        suma_ponderada += val * (i * 1000);
        suma_total     += val;
    }
    if (!linea_detectada)
        return (ultima_posicion < 3500) ? 0 : 7000;
    ultima_posicion = suma_ponderada / suma_total;
    return ultima_posicion;
}
 
int main(void) {
    bsp_init();
    while (1) {
        int posicion    = leer_posicion();
        float error     = 3500.0f - (float)posicion;
        float correccion = (error * KP)
                         + ((error - error_previo) * KD)
                         + (error_acumulado * KI);
        error_previo     = error;
        error_acumulado += error;
 
        int32_t m_izq = VELOCIDAD_BASE - (int32_t)correccion;
        int32_t m_der = VELOCIDAD_BASE + (int32_t)correccion;
 
        if (!linea_detectada) {
            if (ultima_posicion < 3500) set_motores(0, VELOCIDAD_MAX);
            else                        set_motores(VELOCIDAD_MAX, 0);
        } else {
            set_motores(m_izq, m_der);
        }
    }
}
```

## 7. Comparativa de Algoritmos

| Aspecto | Fase 1: Bang-Bang | Fase 2: PD/PID |
| :--- | :--- | :--- |
| **Tipo de control** | Discreto (ON/OFF) | Continuo (analógico) |
| **Timer usado** | TIM1 (PWMA fijo 100%) | TIM4 (PWM independiente por motor) |
| **Pines de motor** | AIN1/AIN2 como switches digitales | TIM4_CH3/CH4 como salidas PWM |
| **Suavidad de curva** | Brusca (oscilaciones) | Fluida (amortiguada por Kd) |
| **Calibración requerida** | Ninguna | Ajuste iterativo de Kp, Kd, Ki |
| **Velocidad alcanzable** | Limitada por rebotes | Mayor (curvas más suaves) |
| **Tamaño del binario** | < 1 KB | < 2 KB |

## 8. Conclusiones
El proyecto integró con éxito conceptos de sistemas embebidos (registro directo sobre ARM Cortex-M4), electrónica de potencia (topología Low-Side Switch ante fallo de hardware), instrumentación (sensores analógicos QTR-8A) y teoría de control (Bang-Bang y PD/PID). El entorno de desarrollo Bare-Metal sobre Arch Linux con toolchain GNU demuestra el dominio de todo el flujo, desde el código fuente hasta el binario flasheado en el microcontrolador, sin ningún asistente de configuración automática.

El desarrollo en dos fases permitió una comprensión progresiva: la Fase 1 (Bang-Bang) validó el hardware y la arquitectura eléctrica de forma rápida, mientras que la Fase 2 (PD/PID con TIM4) elevó el rendimiento a velocidades superiores gracias al control continuo y al amortiguamiento derivativo. La solución de doble sumidero ante el fallo del Canal B evidencia capacidad de resolución de problemas de ingeniería bajo restricciones reales de hardware.

## 9. Referencias
* STMicroelectronics. (2023). *STM32F401 Reference Manual (RM0368)*. st.com
* Toshiba Semiconductor. (2015). *TB6612FNG Dual DC Motor Driver Datasheet*. toshiba.com
* Pololu Corporation. (2023). *QTR-8A Reflectance Sensor Array User’s Guide*. pololu.com
* ARM Limited. (2021). *Cortex-M4 Technical Reference Manual*. developer.arm.com
* GNU Project. (2024). *GNU Arm Embedded Toolchain Documentation*. developer.arm.com
* Franklin, G., Powell, J., & Emami-Naeini, A. (2019). *Feedback Control of Dynamic Systems (8th ed.)*. Pearson.
