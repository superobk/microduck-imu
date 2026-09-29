# Native GNU Arm GCC is default. clang/lld is the offline-tested alternative.
TOOLCHAIN ?= gcc
CLOCK ?= hsi
TRANSPORT ?= dxl
FAULTS ?= 0
WATCHDOG ?= 0
SPI_MHZ ?= 4
ifeq ($(filter $(SPI_MHZ),1 4),)
$(error SPI_MHZ must be 1 or 4)
endif
ifeq ($(filter $(CLOCK),hsi hse),)
$(error CLOCK must be hsi or hse)
endif
ifeq ($(filter $(TRANSPORT),dxl uart1),)
$(error TRANSPORT must be dxl or uart1)
endif
OUT := build/$(CLOCK)-$(TRANSPORT)-f$(FAULTS)-w$(WATCHDOG)-spi$(SPI_MHZ)
SRC := src/startup.c src/board_g031.c src/imu.c src/dxl.c src/main.c
OBJ := $(patsubst src/%.c,$(OUT)/%.o,$(SRC))
CPU := -mcpu=cortex-m0plus -mthumb
CFLAGS := $(CPU) -std=c11 -Os -g3 -Wall -Wextra -Werror -ffreestanding -fno-builtin -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables -Iinclude
CFLAGS += -DHSE_BYPASS=$(if $(filter hse,$(CLOCK)),1,0) -DUART1_BRIDGE=$(if $(filter uart1,$(TRANSPORT)),1,0) -DENABLE_FAULTS=$(FAULTS) -DWATCHDOG=$(WATCHDOG) -DSPI_MHZ=$(SPI_MHZ)
LDFLAGS := $(CPU) -nostdlib -Wl,--gc-sections,-Map=$(OUT)/imu.map -Tstm32g031f8.ld
ifeq ($(TOOLCHAIN),clang)
CC := clang
OBJCOPY := llvm-objcopy
CFLAGS += --target=arm-none-eabi
LDFLAGS += --target=arm-none-eabi -fuse-ld=lld
else
CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
LDFLAGS += -lgcc
endif
.PHONY: all clean test
all: $(OUT)/imu.elf $(OUT)/imu.bin $(OUT)/imu.hex
$(OUT)/%.o: src/%.c include/board.h include/imu.h include/dxl.h Makefile
	mkdir -p $(OUT)
	$(CC) $(CFLAGS) -c $< -o $@
$(OUT)/imu.elf: $(OBJ) stm32g031f8.ld
	$(CC) $(OBJ) $(LDFLAGS) -o $@
$(OUT)/imu.bin: $(OUT)/imu.elf
	$(OBJCOPY) -O binary $< $@
$(OUT)/imu.hex: $(OUT)/imu.elf
	$(OBJCOPY) -O ihex $< $@
test:
	python3 -m unittest discover -s tests -v
clean:
	rm -rf build
