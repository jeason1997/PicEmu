EXAMPLES := blink button buzzer fur_elise

.PHONY: all clean $(EXAMPLES)

all: $(EXAMPLES)

$(EXAMPLES):
	$(MAKE) -C $@ firmware

clean:
	@for example in $(EXAMPLES); do \
		$(MAKE) -C $$example clean; \
	done
