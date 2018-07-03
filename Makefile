CC = gcc
CFLAGS = -Wall
LDLIBS = -pthread -lopus
TARGET = phone phone_pa

all: $(TARGET)

phone: phone.o net.o send_receive_sox.o
	$(CC) -o $@ $^ $(LDLIBS)
phone_pa: phone.o net.o send_receive_pulseaudio.o
	$(CC) -o $@ $^ $(LDLIBS) -lpulse -lpulse-simple

.PHONY: clean tmpclean
tmpclean:
	rm -f *~
clean: tmpclean
	rm $(TARGET) *.o
