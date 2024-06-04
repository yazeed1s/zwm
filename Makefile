CC = gcc
CFLAGS = -Wall -Wextra -Wshadow -Wunreachable-code -Wcast-align -Wuninitialized
LDFLAGS = -lxcb -lxcb-util -lxcb-keysyms -lxcb-ewmh -lxcb-icccm

SRC_FILES = ./src/zwm.c ./src/logger.c ./src/tree.c ./src/config_parser.c
HEADER_FILES = ./src/logger.h ./src/tree.h ./src/type.h ./src/zwm.h ./src/config_parser.h ./src/helper.h
OBJ_FILES = $(SRC_FILES:.c=.o)
DEBUG_FLAGS = -g -O0 -D_DEBUG__=1

TARGET = zwm
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin


$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADER_FILES)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ_FILES)

install: $(TARGET)
	cp -pf $(TARGET) $(BINDIR)

uninstall:
	rm -f $(BINDIR)/$(TARGET)

debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET)

all: clean $(TARGET)

.PHONY: all clean install uninstall debug
