CC = gcc
CFLAGS = -Wall -g
LDLIBS = -pthread -lopus
TARGET = phone phone_pa

all: $(TARGET)

phone: phone.o net.o send_receive_sox.o
	$(CC) -o $@ $^ $(LDLIBS)
phone_pa: phone.o net.o send_receive_pulseaudio.o
	$(CC) -o $@ $^ $(LDLIBS) -lpulse -lpulse-simple

test: test/opus_encode test/opus_decode test/epoll
test/opus_encode: test/opus_encode.o
	$(CC) -o $@ $^ $(LDLIBS)
test/opus_decode: test/opus_decode.o
	$(CC) -o $@ $^ $(LDLIBS)
test/epoll: test/epoll.o
	$(CC) -o $@ $^ $(LDLIBS)

.PHONY: clean tmpclean
tmpclean:
	rm -f *~
clean: tmpclean
	rm $(TARGET) *.o
