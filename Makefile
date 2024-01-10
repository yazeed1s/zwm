CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lxcb -lxcb-util -lxcb-keysyms

SRC_FILES = ./src/zwm.c ./src/logger.c ./src/tree.c
HEADER_FILES = ./src/logger.h ./src/tree.h ./src/type.h ./src/zwm.h
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