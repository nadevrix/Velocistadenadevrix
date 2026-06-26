# Velocista Bare-Metal (STM32F4)

Este repositorio contiene el firmware para un robot seguidor de línea (velocista) de alto rendimiento, programado enteramente en lenguaje C puro (Bare-Metal) sobre un microcontrolador STM32F401 / STM32F411 (BlackPill). 

El proyecto prescinde intencionalmente de librerías de abstracción de hardware (como HAL o entornos tipo Arduino) para garantizar un control determinista sobre los periféricos y mantener la latencia de procesamiento próxima a cero.

## Características Técnicas Principales
* **Desarrollo Bare-Metal:** Configuración directa de registros físicos (RCC, GPIO, ADC, TIM) utilizando punteros a memoria.
* **Algoritmos de Control Intercambiables:** 
  * Control discreto (Bang-Bang por zonas) para máxima agresividad en pista.
  * Control continuo Proporcional-Derivativo (PD/PID) apoyado en PWM generado por hardware.
* **Topología Low-Side Switching ("MacGyver Mode"):** Ante una falla de hardware en un canal del puente H (TB6612FNG), el algoritmo fue modificado para controlar dos motores independientes utilizando un único canal operativo como sumidero de corriente.
* **Calibración Estática Ultra-rápida:** Omisión de rutinas de barrido espacial (auto-calibración dinámica) a favor de un umbral de binarización duro por hardware, logrando un tiempo de arranque (boot-to-action) instantáneo.
* **Optimización de Memoria:** El binario compilado (.bin) ocupa menos de 2 KB de memoria Flash.

## Especificaciones de Hardware
* **Microcontrolador:** STM32F401 / STM32F411 (BlackPill).
* **Driver de Potencia:** Toshiba TB6612FNG (Puente H Dual MOSFET).
* **Matriz de Sensores:** Pololu QTR-8A (Barra Infrarroja Analógica de 8 canales).
* **Actuadores:** 2x Motorreductores Micro Metal N20.
* **Alimentación:** 4 Baterías AA en serie (6V nominales) para la etapa de potencia; regulación on-board a 3.3V para la lógica de la STM32.

## Mapa de Pines (Topología Modificada)
Debido a la inhabilitación del Canal B del puente H, se emplean los pines `AO1` y `AO2` como sumideros para el circuito de los motores.

| Pin STM32 | Periférico | Función Asignada |
| :--- | :--- | :--- |
| **PA0 – PA7** | ADC1 | Entradas de los sensores QTR-8A (S1 a S8). |
| **PA8** | GPIO / TIM1 | PWMA (Señal de habilitación constante al 100%). |
| **PB8** | GPIO / TIM4_CH3 | AIN1 (Línea de control del Motor Derecho). |
| **PB9** | GPIO / TIM4_CH4 | AIN2 (Línea de control del Motor Izquierdo). |
| **3.3V** | Alimentación | Señal STBY del TB6612FNG (Fijado a HIGH). |

## Entorno de Construcción (Build System)
El firmware está diseñado para ser compilado en un entorno Linux (ej. Arch Linux, Ubuntu) utilizando las herramientas estándar de desarrollo embebido GNU.

### Dependencias:
* GNU Arm Embedded Toolchain (`arm-none-eabi-gcc`).
* `make`.
* Herramientas de flasheo (`stlink` o `dfu-util`).

### Instrucciones de Compilación:
1. Clonar el repositorio localmente:
   ```bash
   git clone git@github.com:nadevrix/Velocistadenadevrix.git
   cd Velocistadenadevrix
   ```
2. Compilar el firmware y generar los binarios:
   ```bash
   make
   ```
3. Cargar el firmware al microcontrolador (Requiere modo BOOT0 activo si se emplea DFU):
   ```bash
   sudo make flash
   ```
4. Limpiar los archivos generados:
   ```bash
   make clean
   ```

## Licencia y Autoría
Proyecto académico desarrollado por **Rodrigo Ricaldez Martínez** (Junio 2026).
Diseñado para la asignatura de Microcontroladores.
