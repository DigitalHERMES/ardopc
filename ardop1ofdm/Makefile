#	ARDOPC Makefile
PREFIX=/usr
OBJS = ofdm.o LinSerial.o KISSModule.o pktARDOP.o pktSession.o BusyDetect.o i2cDisplay.o ALSASound.o ARDOPC.o ardopSampleArrays.o ARQ.o FFT.o FEC.o HostInterface.o Modulate.o rs.o berlekamp.o galois.o SoundInput.o TCPHostInterface.o SCSHostInterface.o

# Configuration:
CFLAGS = -DLINBPQ -MMD -g
CC = gcc

all: ardopc

ardopc: $(OBJS)
	gcc $(OBJS) -lrt -lm -lpthread -lasound -o ardop1ofdm

install: ardop1ofdm
	install ardop1ofdm $(PREFIX)/bin
	ln -sf $(PREFIX)/bin/ardop1ofdm $(PREFIX)/bin/ardop
#	install ../initscripts/ardop.service /etc/systemd/system

-include *.d

clean :
	rm ardop1ofdm *.o
