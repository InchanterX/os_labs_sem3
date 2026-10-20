CC     = gcc
CFLAGS = -Wall -Wextra -O2 -g -pthread
SRC    = src

TARGET = search

all: $(TARGET)

$(TARGET): $(SRC)/main.c $(SRC)/dynamic.c $(SRC)/dynamic.h
	$(CC) $(CFLAGS) -o $@ $(SRC)/main.c $(SRC)/dynamic.c

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all clean
