AVRDUDE=avrdude

CFLAGS=-Wall -Werror -g -Os -std=gnu99

TARGETS = HOST uno pirmotion

# Host programs
HOST_PROG += testmbus

testmbus_TARGETS = HOST
testmbus_SRC = testmbus.c mbus.c

# Arduino UNO programs
uno_PROG += toggle echo ioshield vmeter

toggle_SRC = toggle.c
echo_SRC += echo.c server.c mbus.c stubs.c
ioshield_SRC += ioshield.c server.c mbus.c stubs.c
vmeter_SRC = vmeter.c

stubs.c_CFLAGS = -ffunction-sections

# Target arduino uno
uno_GNU = avr-
uno_SIZE = --format=avr --mcu=$(uno_MCU)
uno_CPPFLAGS += -DF_CPU=16000000
uno_MCU = atmega328p
uno_CFLAGS += -mmcu=$(uno_MCU)
uno_LDFLAGS += -mmcu=$(uno_MCU) -Wl,--gc-sections

uno_DUDE_NAME=m328p
uno_DUDE_PROG=arduino
uno_DUDE_BAUD=115200
uno_DUDE_PORT=/dev/ttyACM0


# PIR motion programs
pirmotion_PROG = pir-relay

pir-relay_SRC = pir-relay.c

pirmotion_GNU = avr-
pirmotion_SIZE = --format=avr --mcu=$(pirmotion_MCU)
pirmotion_CPPFLAGS += -DF_CPU=1000000
pirmotion_MCU = atmega88pa
pirmotion_CFLAGS += -mmcu=$(pirmotion_MCU)
pirmotion_LDFLAGS += -mmcu=$(pirmotion_MCU) -Wl,--gc-sections

pirmotion_DUDE_NAME=m88p
pirmotion_DUDE_PROG=avrispmkII
pirmotion_DUDE_BAUD=9600
pirmotion_DUDE_PORT=usb


all: realall

#================= Rules =====================

# $1 is prog name
# $2 is target name
define prog_rules
$1_$2_SRC_ALL ?= $$($1_SRC) $$($1_$2_SRC)

$1_$2_OBJ = $$($1_$2_SRC_ALL:%.c=%-$2.o)

$1-$2.elf: $$($1_$2_OBJ)
realall: $1-$2.elf

clean-$1-$2:
	rm -f $1-$2.elf $1-$2.S
	rm -f $$($1_$2_OBJ)
clean: clean-$1-$2
info-$1-$2:
	@echo "PROG: $1 for $2"
	@echo "file: $1-$2.elf"
	@echo "Load with: make load-$1-$2"
	@echo "$1_$2_SRC = $$($1_$2_SRC_ALL)"
	@echo "$1_$2_OBJ = $$($1_$2_OBJ)"
	@echo
info: info-$1-$2
.PHONY: clean-$1-$2 info-$1-$2
endef

# $1 is target name
define target_rules

%-$1.o: %.c
	$$($1_GNU)gcc -o $$@ -c $$< $$(CPPFLAGS) $$($1_CPPFLAGS) $$($$<_CPPFLAGS) $$(CFLAGS) $$($1_CFLAGS) $$($$<_CFLAGS)

%-$1.elf:
	$$($1_GNU)gcc -o $$@ $$(LDFLAGS) $$($1_LDFLAGS) $$($$<_LDFLAGS) $$(LDADD) $$($1_LDADD) $$($$<_LDADD) $$^
	$$($1_GNU)size $$($1_SIZE) $$@

%-$1.S: %-$1.elf
	$$($1_GNU)objdump -d -S $$< > $$@

%-$1.hex: %-$1.elf
	$$($1_GNU)objcopy -O ihex $$< $$@

load-%-$1: %-$1.hex
	$$(AVRDUDE) -p $$($1_DUDE_NAME) -c $$($1_DUDE_PROG) -b $$($1_DUDE_BAUD) -P $$($1_DUDE_PORT) -U flash:w:$$*-$1.hex:i
endef

.PHONY: all realall info clean

$(foreach target,$(TARGETS),$(eval $(call target_rules,$(target))))

$(foreach target,$(TARGETS),$(foreach prog,$($(target)_PROG),$(eval $(call prog_rules,$(prog),$(target)))))
