# =============================================================================
# Makefile for ATmega324P DS3231 RTC / LCD16x2 / UART0 Cooperative RTOS Project
# Outputs (.o, .d, .elf, .hex) are placed in the Debug/ directory.
# =============================================================================

TARGET         := main
MCU            := atmega324p
F_CPU          := 8000000UL
BUILD_DIR      := Debug

# AVR Toolchain directory (override on command line with AVR_TOOLS_PATH=... if needed)
AVR_TOOLS_PATH ?= C:/Program Files (x86)/Atmel/Studio/7.0/toolchain/avr8/avr8-gnu-toolchain/bin

CC             := "$(AVR_TOOLS_PATH)/avr-gcc.exe"
OBJCOPY        := "$(AVR_TOOLS_PATH)/avr-objcopy.exe"
OBJDUMP        := "$(AVR_TOOLS_PATH)/avr-objdump.exe"
SIZE           := "$(AVR_TOOLS_PATH)/avr-size.exe"

# Source directories
SRC_DIRS := \
    src \
    src/dev/M0_App \
    src/dev/M1_Sys/CommandLine \
    src/dev/M1_Sys/Scheduler \
    src/dev/M1_Sys/SystemData \
    src/dev/M2_BSP \
    src/dev/M2_BSP/DS3231 \
    src/dev/M2_BSP/I2C \
    src/dev/M2_BSP/LCD \
    src/dev/M2_BSP/LED \
    src/dev/M2_BSP/TIMER \
    src/dev/M2_BSP/UART \
    src/dev/M3_Driver/Component/DIO \
    src/dev/M3_Driver/Component/I2C \
    src/dev/M3_Driver/Component/UART \
    src/dev/M3_Driver/Component/UART0 \
    src/dev/M3_Driver/Devices/DS3231 \
    src/dev/M3_Driver/Devices/LCD16x2 \
    src/dev/M4_Middleware/FIFO

# Source files
SRCS := \
    src/main.c \
    src/dev/M0_App/app_main.c \
    src/dev/M0_App/app_clock.c \
    src/dev/M0_App/app_console.c \
    src/dev/M0_App/app_lcd.c \
    src/dev/M0_App/app_led.c \
    src/dev/M0_App/app_temp.c \
    src/dev/M1_Sys/CommandLine/cmdline.c \
    src/dev/M1_Sys/CommandLine/command.c \
    src/dev/M1_Sys/CommandLine/command_led.c \
    src/dev/M1_Sys/CommandLine/command_time.c \
    src/dev/M1_Sys/CommandLine/command_temp.c \
    src/dev/M1_Sys/Scheduler/scheduler.c \
    src/dev/M1_Sys/SystemData/system_data.c \
    src/dev/M2_BSP/DS3231/bsp_ds3231.c \
    src/dev/M2_BSP/I2C/bsp_i2c.c \
    src/dev/M2_BSP/LCD/bsp_lcd.c \
    src/dev/M2_BSP/LED/bsp_led.c \
    src/dev/M2_BSP/TIMER/bsp_timer.c \
    src/dev/M2_BSP/UART/bsp_uart.c \
    src/dev/M3_Driver/Component/DIO/do.c \
    src/dev/M3_Driver/Component/I2C/i2c.c \
    src/dev/M3_Driver/Component/UART/uart.c \
    src/dev/M3_Driver/Component/UART0/uart0.c \
    src/dev/M3_Driver/Devices/DS3231/ds3231.c \
    src/dev/M3_Driver/Devices/LCD16x2/lcd16x2.c \
    src/dev/M4_Middleware/FIFO/fifo.c

# Object (.o) and dependency (.d) files in Debug/
OBJS := $(addprefix $(BUILD_DIR)/,$(notdir $(SRCS:.c=.o)))
DEPS := $(OBJS:.o=.d)

# Search paths for .c source files
vpath %.c $(SRC_DIRS)

# Include flags
INCLUDES := -I. $(addprefix -I,$(SRC_DIRS))

# Compiler and linker flags
CFLAGS := \
    -mmcu=$(MCU) \
    -DF_CPU=$(F_CPU) \
    -Os \
    -g2 \
    -std=gnu11 \
    -Wall \
    -Wextra \
    -ffunction-sections \
    -fdata-sections \
    -fpack-struct \
    -fshort-enums \
    -funsigned-char \
    -funsigned-bitfields \
    $(INCLUDES)

DEPFLAGS = -MMD -MP -MF $(BUILD_DIR)/$*.d

LDFLAGS := \
    -mmcu=$(MCU) \
    -Wl,--gc-sections \
    -Wl,-Map,$(BUILD_DIR)/$(TARGET).map

# Primary outputs in Debug/
ELF := $(BUILD_DIR)/$(TARGET).elf
HEX := $(BUILD_DIR)/$(TARGET).hex

.PHONY: all clean size

all: $(BUILD_DIR) $(HEX) size

$(BUILD_DIR):
	@if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"

# Compile .c -> Debug/*.o and Debug/*.d
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c "$<" -o "$@"

# Link Debug/*.o -> Debug/main.elf
$(ELF): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o "$@"

# Convert Debug/main.elf -> Debug/main.hex (Intel HEX)
$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock -R .signature "$<" "$@"

# Print Flash and SRAM usage
size: $(ELF)
	$(SIZE) --format=avr --mcu=$(MCU) "$(ELF)"

# Remove generated files in Debug/
clean:
	@if exist "$(BUILD_DIR)" del /Q "$(BUILD_DIR)\*.o" "$(BUILD_DIR)\*.d" "$(BUILD_DIR)\*.elf" "$(BUILD_DIR)\*.hex" "$(BUILD_DIR)\*.map" 2>nul

-include $(DEPS)
