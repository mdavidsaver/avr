
CFLAGS=-Wall -Werror

TARGETS = HOST uno pirmotion

# Host programs
HOST_PROG += testmbus

# gnu99 or c11 required due to use of anonymous unions.
HOST_CFLAGS += -g -Wall -std=gnu99

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
uno_CFLAGS += -mmcu=$(uno_MCU) -Wall -g -Os -std=gnu99
uno_LDFLAGS += -mmcu=$(uno_MCU) -Wl,--gc-sections

uno_DUDE_NAME=m328p
uno_DUDE_PROG=arduino
uno_DUDE_BAUD=115200
uno_DUDE_PORT=/dev/ttyACM0



all: realall

# $1 is prog name
# $2 is target name
define prog_rules
$1_$2_SRC_ALL ?= $$($1_SRC) $$($1_$2_SRC)

$1_$2_OBJ = $$($1_$2_SRC_ALL:%.c=%-$2.o)

$1-$2.elf: $$($1_$2_OBJ)
realall: $1-$2.elf

clean-$1-$2:
	rm -f $1-$2.elf
	rm -f $$($1_$2_OBJ)
clean: clean-$1-$2
info-$1-$2:
	@echo "PROG: $1 for $2"
	@echo "file: $1-$2.elf"
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
endef

.PHONY: all realall info clean

$(foreach target,$(TARGETS),$(eval $(call target_rules,$(target))))

#$(foreach prog,$(HOST_PROG),$(eval $(call prog_rules,$(prog),HOST)))

$(foreach target,$(TARGETS),$(foreach prog,$($(target)_PROG),$(eval $(call prog_rules,$(prog),$(target)))))
