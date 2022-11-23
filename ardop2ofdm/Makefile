#	ARDOPC Makefile
PREFIX=/usr
OBJS = ofdm.o LinSerial.o KISSModule.o pktARDOP.o pktSession.o BusyDetect.o i2cDisplay.o ALSASound.o ARDOPC.o ardopSampleArrays.o ARQ.o FFT.o FEC.o HostInterface.o Modulate.o rs.o berlekamp.o galois.o SoundInput.o TCPHostInterface.o SCSHostInterface.o

# Configuration:
CFLAGS = -DLINBPQ -MMD -g
CC = gcc

all: ardopofdm

ardopofdm: $(OBJS)
	gcc $(OBJS) -lrt -lm -lpthread -lasound -o ardopofdm

install: ardopofdm
	install ardopofdm $(PREFIX)/bin
	ln -sf $(PREFIX)/bin/ardopofdm $(PREFIX)/bin/ardop
#	install ../initscripts/ardop.service /etc/systemd/system

-include *.d

clean :
	rm -f ardopofdm $(OBJS) output.map *.d
