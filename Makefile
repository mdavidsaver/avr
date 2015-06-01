AVRDUDE=avrdude

CFLAGS=-Wall -Werror -g -Os -std=gnu99

TARGETS = HOST uno pirmotion ukey

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
uno_CPPFLAGS += -DF_CPU=16000000
uno_MCU = atmega328p
uno_LDFLAGS += -Wl,--gc-sections

uno_DUDE_PROG=arduino
uno_DUDE_BAUD=115200
uno_DUDE_PORT=/dev/ttyACM0


# PIR motion programs
pirmotion_PROG = pir-relay

pir-relay_SRC = pir-relay.c

pirmotion_GNU = avr-
pirmotion_CPPFLAGS += -DF_CPU=1000000
pirmotion_MCU = atmega88pa

# UKEY programs

ukey_PROG = simpleusb

simpleusb_SRC = simpleusb.c

ukey_GNU = avr-
ukey_CPPFLAGS += -DF_CPU=8000000
ukey_MCU = atmega8u2

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
clean-$1: clean-$1-$2
clean: clean-$1-$2
info-$1-$2:
	@echo "PROG: $1 for $2"
	@echo "file: $1-$2.elf"
ifneq ($2,HOST)
	@echo "Load with: make load-$1-$2"
endif
	@echo "$1_$2_SRC = $$($1_$2_SRC_ALL)"
	@echo "$1_$2_OBJ = $$($1_$2_OBJ)"
	@echo
info-$1: info-$1-$2
info: info-$1-$2
.PHONY: clean-$1-$2 info-$1-$2
endef

# $1 is target name
define target_rules

ifneq ($1,HOST)
$1_DUDE_PROG=avrispmkII
$1_DUDE_BAUD=9600
$1_DUDE_PORT=usb
$1_SIZE = --format=avr --mcu=$$($1_MCU)
$1_CFLAGS += -mmcu=$$($1_MCU)
$1_LDFLAGS += -mmcu=$$($1_MCU)
endif

%-$1.o: %.c
	$$($1_GNU)gcc -o $$@ -c $$< $$(CPPFLAGS) $$($1_CPPFLAGS) $$($$<_CPPFLAGS) $$(CFLAGS) $$($1_CFLAGS) $$($$<_CFLAGS)

%-$1.elf:
	$$($1_GNU)gcc -o $$@ $$(LDFLAGS) $$($1_LDFLAGS) $$($$<_LDFLAGS) $$(LDADD) $$($1_LDADD) $$($$<_LDADD) $$^
	$$($1_GNU)size $$($1_SIZE) $$@

%-$1.S: %-$1.elf
	$$($1_GNU)objdump -d -S $$< > $$@

%-$1.hex: %-$1.elf
	$$($1_GNU)objcopy -O ihex $$< $$@

ifneq ($1,HOST)
load-%-$1: %-$1.hex
	$$(AVRDUDE) -p $$(DUDE_$$($1_MCU)) -c $$($1_DUDE_PROG) -b $$($1_DUDE_BAUD) -P $$($1_DUDE_PORT) -U flash:w:$$*-$1.hex:i

# Read fused settings
fuse-$1:
	$$(AVRDUDE) -p $$(DUDE_$$($1_MCU)) -c $$($1_DUDE_PROG) -b $$($1_DUDE_BAUD) -P $$($1_DUDE_PORT) -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h -U lock:r:-:h

endif
endef

.PHONY: all realall info clean

$(foreach target,$(TARGETS),$(eval $(call target_rules,$(target))))

$(foreach target,$(TARGETS),$(foreach prog,$($(target)_PROG),$(eval $(call prog_rules,$(prog),$(target)))))

# mapping between GCC and avrdude chip names
DUDE_atmega328p = m328p
DUDE_atmega88pa = m88p
DUDE_atmega8u2  = m8u2
