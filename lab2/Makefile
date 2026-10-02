CC     = gcc
CFLAGS = -Wall -Wextra -g
SRC    = lab2/src

TARGETS = parent lower imploder

all: $(TARGETS)

parent: $(SRC)/parent.c $(SRC)/writer.c $(SRC)/writer.h
	$(CC) $(CFLAGS) -o $@ $(SRC)/parent.c $(SRC)/writer.c

lower: $(SRC)/lower.c $(SRC)/writer.c $(SRC)/writer.h
	$(CC) $(CFLAGS) -o $@ $(SRC)/lower.c $(SRC)/writer.c

imploder: $(SRC)/imploder.c $(SRC)/writer.c $(SRC)/writer.h
	$(CC) $(CFLAGS) -o $@ $(SRC)/imploder.c $(SRC)/writer.c

clean:
	rm -f $(TARGETS) $(addsuffix .exe,$(TARGETS))

.PHONY: all clean
