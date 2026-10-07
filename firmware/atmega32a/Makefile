MCU = atmega32
F_CPU = 8000000UL
TARGET = robot_arm_atmega32a

CC = avr-gcc
OBJCOPY = avr-objcopy
SIZE = avr-size
AVRDUDE = avrdude

SRC = src/main.c src/timebase.c src/uart.c src/twi.c src/pca9685.c src/limit.c src/servo.c
OBJ = $(SRC:.c=.o)
SMOKE_TARGET = servo_smoke_test
SMOKE_SRC = src/smoke_test.c src/timebase.c src/twi.c src/pca9685.c
SMOKE_OBJ = $(SMOKE_SRC:.c=.smoke.o)

CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -std=gnu99 -Os -Wall -Wextra \
         -ffunction-sections -fdata-sections -Iinclude
LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections,-Map=$(TARGET).map

.PHONY: all clean flash smoke-test flash-smoke read-fuses

all: $(TARGET).hex

$(TARGET).elf: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)
	$(SIZE) --format=avr --mcu=$(MCU) $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

flash: $(TARGET).hex
	$(AVRDUDE) -c usbasp -p m32 -U flash:w:$(TARGET).hex:i

smoke-test: $(SMOKE_TARGET).hex

$(SMOKE_TARGET).elf: $(SMOKE_OBJ)
	$(CC) $(LDFLAGS) -o $@ $(SMOKE_OBJ)
	$(SIZE) --format=avr --mcu=$(MCU) $@

$(SMOKE_TARGET).hex: $(SMOKE_TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

src/%.smoke.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

flash-smoke: $(SMOKE_TARGET).hex
	$(AVRDUDE) -c usbasp -p m32 -U flash:w:$(SMOKE_TARGET).hex:i

read-fuses:
	$(AVRDUDE) -c usbasp -p m32 -U lfuse:r:-:h -U hfuse:r:-:h

clean:
	rm -f $(OBJ) $(SMOKE_OBJ) $(TARGET).elf $(TARGET).hex $(TARGET).map \
	      $(SMOKE_TARGET).elf $(SMOKE_TARGET).hex
