APPNAME=msp430g2553_bootloader
MCU=msp430g2553

OBJECTS :=
OBJECTS += main.o

GCC_PATH=/opt/local
SUPPORT_PATH=/opt/local/msp430

CC=${GCC_PATH}/bin/msp430-gcc
OBJCOPY=${GCC_PATH}/bin/msp430-objcopy

# LINKER_SCRIPT = -Wl,--section-start -Wl,-Tmsp430.x
LINKER_SCRIPT = -T msp430.x

# CFLAGS= -mmcu=${MCU} -g -Os -Wall -Wunused
CFLAGS= -mmcu=${MCU} ${LINKER_SCRIPT} -g -Os -Wall -Wunused
LDLIBS = -I${SUPPORT_PATH}/include

all: ${APPNAME}.hex

%.o: %.c
	${CC} ${CFLAGS} ${LDLIBS} -c -o $@ $<

${APPNAME}.elf: ${OBJECTS}
	${CC} -mmcu=${MCU} -o $@ $^

${APPNAME}.hex: ${APPNAME}.elf
	${OBJCOPY} --gap-fill 0xFF -O ihex $< $@

clean:
	rm *.o ${APPNAME}.elf ${APPNAME}.hex
