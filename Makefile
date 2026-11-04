CC = gcc
CFLAGS = -Wall -Wextra -std=c17 -pthread -Iinclude
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
TARGET = torneo

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean