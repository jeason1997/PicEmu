EXAMPLES := blink breathing_led button buzzer led_chaser playmusic spi_flash

.PHONY: all clean $(EXAMPLES)

all: $(EXAMPLES)

$(EXAMPLES):
	$(MAKE) -C $@ firmware

clean:
	@for example in $(EXAMPLES); do \
		$(MAKE) -C $$example clean; \
	done
