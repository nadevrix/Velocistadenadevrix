# ============================================================
#  Makefile - Velocista STM32F401/F411 (BlackPill) Bare-Metal
# ============================================================

# Toolchain
CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# Project
TARGET  = velocista
SRCS    = main.c startup.c
OBJS    = $(SRCS:.c=.o)

# MCU flags (Cortex-M4 con FPU)
CPU     = -mcpu=cortex-m4
FPU     = -mfloat-abi=hard -mfpu=fpv4-sp-d16
MCU     = $(CPU) -mthumb $(FPU)

# Compiler flags
CFLAGS  = $(MCU) -std=c99 -Os -Wall -Wextra
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -fno-exceptions -fno-builtin
CFLAGS += -DSTM32F401xE

# Linker flags
LDSCRIPT = stm32f4.ld
LDFLAGS  = $(MCU) -T$(LDSCRIPT) -Wl,--gc-sections
LDFLAGS += -nostdlib -nostartfiles
LDFLAGS += -Wl,-Map=$(TARGET).map

# DFU serial number (de tu STM32 conectado)
DFU_SERIAL = 366138623030

# ============================================================
#  Build Rules
# ============================================================

all: $(TARGET).bin size

# Link
$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

# Binary for DFU
$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# Compile
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Size info
size: $(TARGET).elf
	$(SIZE) $<

# ============================================================
#  Flash via DFU
#  NOTA: Ejecuta "make flash" con sudo tú mismo:
#    sudo make flash
# ============================================================

flash: $(TARGET).bin
	@echo "============================================"
	@echo "  Flashing $(TARGET).bin via DFU..."
	@echo "  Serial: $(DFU_SERIAL)"
	@echo "============================================"
	dfu-util -a 0 -S $(DFU_SERIAL) -s 0x08000000:leave -D $(TARGET).bin

# ============================================================
#  Clean
# ============================================================

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin $(TARGET).hex $(TARGET).map

.PHONY: all clean flash size
