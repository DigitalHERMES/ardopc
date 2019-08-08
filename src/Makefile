#	ARDOPC Makefile
PREFIX=/usr
OBJS = ofdm.o LinSerial.o KISSModule.o pktARDOP.o pktSession.o BusyDetect.o i2cDisplay.o ALSASound.o ARDOPC.o ardopSampleArrays.o ARQ.o FFT.o FEC.o HostInterface.o Modulate.o rs.o berlekamp.o galois.o SoundInput.o TCPHostInterface.o SCSHostInterface.o

# Configuration:
CFLAGS = -DLINBPQ -MMD -g
CC = gcc

all: ardopc

ardopc: $(OBJS)
	gcc $(OBJS) -Xlinker -Map=output.map -lrt -lm -lpthread -lasound -o ardop1ofdm

install: ardop1ofdm
	install ardop1ofdm $(PREFIX)/bin
	install ../initscripts/ardop.service /etc/systemd/system

-include *.d

clean :
	rm ardop1ofdm $(OBJS)

