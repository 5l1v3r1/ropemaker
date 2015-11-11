CC = xtensa-lx106-elf-gcc
CFLAGS = -I. -mlongcalls
LDLIBS = -nostdlib -Wl,--start-group -lmain -lnet80211 -lwpa -llwip -lpp -lphy -Wl,--end-group -lgcc
LDFLAGS = -Teagle.app.v6.ld

ropemaker-0x00000.bin: ropemaker
	esptool.py elf2image $^

ropemaker: ropemaker.o webapi.o

ropemaker.o: ropemaker.c rope.h

webapi.o: webapi.c rope.h

flash: ropemaker-0x00000.bin
	esptool.py -p /dev/ttyUSB0 -b 230400 write_flash 0x00000 ropemaker-0x00000.bin 0x40000 ropemaker-0x40000.bin
