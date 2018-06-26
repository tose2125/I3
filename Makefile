CC = gcc
CFLAGS = -Wall
LDLIBS = -pthread -lpulse -lpulse-simple
TARGET = phone

all: $(TARGET)

phone: phone.o net.o
	$(CC) -o $@ $^ $(LDLIBS)

.PHONY: clean tmpclean
tmpclean:
	rm -f *~
clean: tmpclean
	rm $(TARGET) *.o
