
-include config.local

TARGET_PREFIX?=avr-

DUDE?=avrdude

all: host-all target-all

info: host-info target-info

clean: host-clean target-clean

.phony: host target clean host-clean target-clean

host-%:
	$(MAKE) -f Makefile.build $* MODE=HOST

target-%:
	$(MAKE) -f Makefile.build $* MODE=TARGET \
"CC=$(TARGET_PREFIX)gcc" \
"OBJCOPY=$(TARGET_PREFIX)objcopy" \
"DUMP=$(TARGET_PREFIX)objdump" \
"SIZE=$(TARGET_PREFIX)size" \
DUDE=$(DUDE)
