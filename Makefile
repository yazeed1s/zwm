CC = gcc
CFLAGS = -Wall -Wextra -Wshadow -Wunreachable-code -Wcast-align -Wuninitialized -g -ggdb
LDFLAGS = -lxcb -lxcb-util -lxcb-keysyms -lxcb-ewmh -lxcb-icccm

SRC_FILES = ./src/zwm.c ./src/logger.c ./src/tree.c ./src/config_parser.c
HEADER_FILES = ./src/logger.h ./src/tree.h ./src/type.h ./src/zwm.h ./src/config_parser.h
OBJ_FILES = $(SRC_FILES:.c=.o)

TARGET = zwm

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADER_FILES)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ_FILES)

all: clean $(TARGET)

.PHONY: all clean
