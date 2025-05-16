CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 vte-2.91` -Wall -g
LDFLAGS = `pkg-config --libs gtk+-3.0 vte-2.91`

SRC = src/main.c
TARGET = quiltt

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/usr/bin/$(TARGET)
	install -Dm644 quiltt.desktop $(DESTDIR)/usr/share/applications/quiltt.desktop

.PHONY: all clean install