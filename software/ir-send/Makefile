TARGET := ir-send
CC := avr-gcc
OBJCOPY := avr-objcopy
CFLAGS := -Wall -Os -DF_CPU=16000000UL -mmcu=atmega328p

all: $(TARGET).hex
$(TARGET).hex: $(TARGET)
	$(OBJCOPY) -O ihex -R .eeprom $< $@
$(TARGET): $(TARGET).o
	$(CC) $(LDFLAGS) -o $@ $<
$(TARGET).o: $(TARGET).c
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: install
install: $(TARGET).hex
	avrdude -F -V -c arduino -p ATMEGA328P -P /dev/ttyACM0 -b 115200 -U flash:w:$(TARGET).hex

.PHONY: clean
clean:
	rm -f $(TARGET).hex $(TARGET) $(TARGET).o *~
