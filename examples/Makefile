EXAMPLES := blink breathing_led button buzzer i2c_lcd1602 led_chaser playmusic seven_segment spi_flash

.PHONY: all clean $(EXAMPLES)

all: $(EXAMPLES)

$(EXAMPLES):
	$(MAKE) -C $@ firmware

clean:
	@for example in $(EXAMPLES); do \
		$(MAKE) -C $$example clean; \
	done
