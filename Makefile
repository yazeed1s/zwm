# compiler and basic flags
CC = gcc
CFLAGS = -Wextra -Wshadow -Wunreachable-code -Wcast-align -Wuninitialized \
         -finline-functions -finline-small-functions \
         -Wno-unused-variable -Wno-unused-function
LDFLAGS = -lxcb -lxcb-util -lxcb-keysyms -lxcb-ewmh -lxcb-icccm \
          -lxcb-randr -lxcb-xinerama -lxcb-cursor -lm

# project structure
TARGET = zwm
SRC_DIR = ./src
SRC_FILES = $(SRC_DIR)/zwm.c $(SRC_DIR)/logger.c $(SRC_DIR)/tree.c \
            $(SRC_DIR)/config_parser.c $(SRC_DIR)/queue.c $(SRC_DIR)/drag.c
HEADER_FILES = $(SRC_DIR)/logger.h $(SRC_DIR)/tree.h $(SRC_DIR)/type.h \
               $(SRC_DIR)/zwm.h $(SRC_DIR)/config_parser.h $(SRC_DIR)/helper.h \
               $(SRC_DIR)/queue.h $(SRC_DIR)/drag.h
OBJ_FILES = $(SRC_FILES:.c=.o)

# paths
PREFIX = /usr
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1
DATADIR = $(PREFIX)/share/zwm
MANPAGE = zwm.1
TEMPLATE = zwm.conf

# build variant flags
DEBUG_FLAGS = -g -D_DEBUG__=1
GDB_FLAGS = -ggdb3 -O0 -fno-omit-frame-pointer
ASAN_FLAGS = -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer -g -O0
TEST_FLAGS = -D__LTEST__=1

.DEFAULT_GOAL := release

# release build
release: CFLAGS += -O2 -DNDEBUG -flto=auto -ffunction-sections -fdata-sections
release: LDFLAGS += -flto=auto -Wl,--gc-sections -s
release: $(TARGET)

# debug build
debug: CFLAGS += $(DEBUG_FLAGS) -O0 -Wno-unused-variable -Wno-unused-function
debug: clean $(TARGET)

# gdb
gdb: CFLAGS += $(GDB_FLAGS)
gdb: clean $(TARGET)

# not used
asan: CFLAGS += $(ASAN_FLAGS)
asan: LDFLAGS += $(ASAN_FLAGS)
asan: clean $(TARGET)

# test build
test: CFLAGS += $(TEST_FLAGS) $(DEBUG_FLAGS)
test: clean $(TARGET)

# 'all' target (same as release)
all: release

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADER_FILES)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJ_FILES)

install: clean release
	mkdir -p "$(DESTDIR)$(BINDIR)"
	cp -pf $(TARGET) "$(DESTDIR)$(BINDIR)"
	mkdir -p "$(DESTDIR)$(MANDIR)"
	cp -pf $(MANPAGE) "$(DESTDIR)$(MANDIR)"
	mkdir -p "$(DESTDIR)$(DATADIR)"
	cp -pf $(TEMPLATE) "$(DESTDIR)$(DATADIR)"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(TARGET)"
	rm -f "$(DESTDIR)$(MANDIR)/$(MANPAGE)"
	rm -f "$(DESTDIR)$(DATADIR)/$(TEMPLATE)"
	rmdir "$(DESTDIR)$(DATADIR)" 2>/dev/null || true

# print build config
info:
	@echo "TARGET:       $(TARGET)"
	@echo "CC:           $(CC)"
	@echo "CFLAGS:       $(CFLAGS)"
	@echo "LDFLAGS:      $(LDFLAGS)"
	@echo "SRC_FILES:    $(SRC_FILES)"
	@echo "OBJ_FILES:    $(OBJ_FILES)"

.PHONY: all release debug gdb asan test clean install uninstall info
