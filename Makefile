############################################
## Makefile for the project mpx/ppm-2-spi ##
############################################

## General Flags
MCU = atmega328p
TARGET = ppm2spi

CC  = avr-gcc
CPP = avr-g++

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON) -DPROTO
CFLAGS += -Wall -gdwarf-2 -std=gnu99 -DPROTO_HAS_CC2500 -DUNIMOD=2 -DF_CPU=11059200UL -Os -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -fno-jump-tables  
#CFLAGS += -MD -MP -MT $(*F).o -MF $(BUILD_DIR)/$(*F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=ppm2spi.map

## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .fuse -R .lock -R .signature

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings

SRC_DIR = src
BUILD_DIR = build

SOURCES := $(wildcard $(SRC_DIR)/*.c)
O_FILES =  $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o, $(SOURCES))
#O_FILES = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
HEADERS = $(wildcard $(SRC_DIR)/*.h)
SOURCE_FILE = $(patsubst $(BUILD_DIR)/%.o,$(SRC_DIR)/%.c, $@)

## Objects explicitly added by the user.
LINKONLYOBJECTS = 
LIBDIRS =
LIBS =

## Partial build
default: $(TARGET).elf

## Compile
# .SECONDEXPANSION:
# $(O_FILES): $$(patsubst $(BUILD_DIR)/%.o,$(SRC_DIR)/%.c, $$@)
#	@echo COMPILING $(SOURCE_FILE) 
#	@$(CC) $(INCLUDES) $(CFLAGS) -c -o $@  $(SOURCE_FILE)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@echo COMPILING $< 
	@$(CC) $(INCLUDES) $(CFLAGS) -c -o $@  $<

$(O_FILES): $(HEADERS) Makefile

## Link
$(TARGET).elf: $(O_FILES)
	@echo LINKING $(O_FILES)
	@$(CC) $(LDFLAGS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $@  $^ # $^ Link all objects regardless of age.

.PHONY: hex eep lss clean all gccversion

hex: $(TARGET).elf
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $<  $(TARGET).hex

eep: $(TARGET).elf
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex  $<  $(TARGET).eep || exit 0

lss: $(TARGET).elf
	avr-objdump -h -S  $<  > $(TARGET).lss

## Clean target
clean:
	-rm -f  $(BUILD_DIR)/*.o ppm2spi.elf ppm2spi.hex ppm2spi.eep ppm2spi.lss ppm2spi.map

## Complete Rebuild 
all: clean gccversion hex eep lss

## Display compiler version.
gccversion:
	@$(CC) --version

	
