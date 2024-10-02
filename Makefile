CC = gcc
CFLAGS = -Wextra -Wshadow -Wunreachable-code -Wcast-align -Wuninitialized -finline-functions -finline-small-functions -findirect-inlining -fstrict-aliasing -falign-functions -falign-jumps -falign-labels -falign-loops -fprefetch-loop-arrays -fdump-rtl-loop2

LDFLAGS = -lxcb -lxcb-util -lxcb-keysyms -lxcb-ewmh -lxcb-icccm -lxcb-randr -lxcb-xinerama -lxcb-cursor
SRC_FILES = ./src/zwm.c ./src/logger.c ./src/tree.c ./src/config_parser.c
HEADER_FILES = ./src/logger.h ./src/tree.h ./src/type.h ./src/zwm.h ./src/config_parser.h ./src/helper.h
OBJ_FILES = $(SRC_FILES:.c=.o)
DEBUG_FLAGS = -g -O0 -D_DEBUG__=1
GDB_FLAGS = -ggdb3
LOCAL_TEST = -D__LTEST__=1
TARGET = zwm
PREFIX = /usr
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1
MANPAGE = zwm.1

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADER_FILES)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ_FILES)

install: $(TARGET)
	mkdir -p "$(DESTDIR)$(BINDIR)"
	cp -pf $(TARGET) "$(DESTDIR)$(BINDIR)"
	mkdir -p "$(DESTDIR)$(MANDIR)"
	cp -pf $(MANPAGE) "$(DESTDIR)$(MANDIR)"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(TARGET)"
	rm -f "$(DESTDIR)$(MANDIR)/$(MANPAGE)"

debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET)

for_gdb: CFLAGS += $(GDB_FLAGS)
for_gdb: clean $(TARGET)

test: CFLAGS += $(LOCAL_TEST)
test: clean $(TARGET)

all: clean $(TARGET)

.PHONY: all clean install uninstall debug
