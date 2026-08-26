CC = gcc
CFLAGS = -Wextra -Wshadow -Wunreachable-code -Wcast-align -Wuninitialized \
         -Wno-unused-variable -Wno-unused-function
LDFLAGS = -Wl,--as-needed -lxcb -lxcb-util -lxcb-keysyms -lxcb-ewmh -lxcb-icccm \
          -lxcb-randr -lxcb-xinerama -lxcb-cursor

TARGET = zwm
SRC_DIR = ./src
TOOLS_DIR = ./tools
SRC_FILES = $(SRC_DIR)/actions.c $(SRC_DIR)/bindings.c $(SRC_DIR)/client.c \
            $(SRC_DIR)/config_parser.c $(SRC_DIR)/cursor.c $(SRC_DIR)/desktop.c \
            $(SRC_DIR)/drag.c $(SRC_DIR)/events.c $(SRC_DIR)/ewmh.c \
            $(SRC_DIR)/focus.c $(SRC_DIR)/logger.c $(SRC_DIR)/monitor.c \
            $(SRC_DIR)/mouse.c $(SRC_DIR)/queue.c $(SRC_DIR)/stacking.c \
            $(SRC_DIR)/state.c $(SRC_DIR)/tree.c $(SRC_DIR)/layout.c $(SRC_DIR)/xcb_util.c \
            $(SRC_DIR)/view.c $(SRC_DIR)/zwm.c
HEADER_FILES = $(SRC_DIR)/actions.h $(SRC_DIR)/bindings.h $(SRC_DIR)/client.h \
               $(SRC_DIR)/config_parser.h $(SRC_DIR)/cursor.h $(SRC_DIR)/desktop.h \
               $(SRC_DIR)/drag.h $(SRC_DIR)/events.h $(SRC_DIR)/ewmh.h \
               $(SRC_DIR)/focus.h $(SRC_DIR)/helper.h $(SRC_DIR)/logger.h \
               $(SRC_DIR)/monitor.h $(SRC_DIR)/mouse.h $(SRC_DIR)/queue.h \
               $(SRC_DIR)/stacking.h $(SRC_DIR)/state.h $(SRC_DIR)/tree.h $(SRC_DIR)/layout.h \
               $(SRC_DIR)/type.h $(SRC_DIR)/xcb_util.h  $(SRC_DIR)/layout.h $(SRC_DIR)/view.h
OBJ_FILES = $(SRC_FILES:.c=.o)
SINGLE_FILE = $(SRC_DIR)/zwm_single.c
SINGLE_TARGET = zwm_single
AMALGAMATE = $(TOOLS_DIR)/merge.pl
LICENSE_FILE = LICENSE

PREFIX = /usr
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1
DATADIR = $(PREFIX)/share/zwm
MANPAGE = zwm.1
TEMPLATE = zwm.conf

DEBUG_FLAGS = -g -D_DEBUG__=1
GDB_FLAGS = -ggdb3 -O0 -fno-omit-frame-pointer
ASAN_FLAGS = -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer -g -O0
TEST_FLAGS = -D__LTEST__=1

.DEFAULT_GOAL := release

release: CFLAGS += -O2 -DNDEBUG -flto=auto -ffunction-sections -fdata-sections -fno-ident
release: LDFLAGS += -flto=auto -Wl,--gc-sections -s
release: clean $(TARGET)

debug: CFLAGS += $(DEBUG_FLAGS) -O0 -Wno-unused-variable -Wno-unused-function
debug: clean $(TARGET)

gdb: CFLAGS += $(GDB_FLAGS)
gdb: clean $(TARGET)

# not used
asan: CFLAGS += $(ASAN_FLAGS)
asan: LDFLAGS += $(ASAN_FLAGS)
asan: clean $(TARGET)

test: CFLAGS += $(TEST_FLAGS) $(DEBUG_FLAGS)
test: clean $(TARGET)

all: release

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c $(HEADER_FILES)
	$(CC) $(CFLAGS) -c $< -o $@

amalgamate: $(SINGLE_FILE)

$(SINGLE_FILE): $(HEADER_FILES) $(SRC_FILES) $(AMALGAMATE) $(LICENSE_FILE)
	$(AMALGAMATE) -o $@ $(HEADER_FILES) -- $(SRC_FILES)

single: CFLAGS += -O2 -DNDEBUG -flto=auto -ffunction-sections -fdata-sections -fno-ident
single: LDFLAGS += -flto=auto -Wl,--gc-sections -s
single: clean $(SINGLE_FILE)
	$(CC) $(CFLAGS) -o $(SINGLE_TARGET) $(SINGLE_FILE) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(SINGLE_TARGET) $(OBJ_FILES)

install: clean release
	mkdir -p "$(DESTDIR)$(BINDIR)"
	cp -pf $(TARGET) "$(DESTDIR)$(BINDIR)"
	mkdir -p "$(DESTDIR)$(MANDIR)"
	cp -pf $(MANPAGE) "$(DESTDIR)$(MANDIR)"
	mkdir -p "$(DESTDIR)$(DATADIR)"
	cp -pf $(TEMPLATE) "$(DESTDIR)$(DATADIR)"
	$(MAKE) clean

install-single: single
	mkdir -p "$(DESTDIR)$(BINDIR)"
	cp -pf $(SINGLE_TARGET) "$(DESTDIR)$(BINDIR)/$(TARGET)"
	mkdir -p "$(DESTDIR)$(MANDIR)"
	cp -pf $(MANPAGE) "$(DESTDIR)$(MANDIR)"
	mkdir -p "$(DESTDIR)$(DATADIR)"
	cp -pf $(TEMPLATE) "$(DESTDIR)$(DATADIR)"
	$(MAKE) clean

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/$(TARGET)"
	rm -f "$(DESTDIR)$(MANDIR)/$(MANPAGE)"
	rm -f "$(DESTDIR)$(DATADIR)/$(TEMPLATE)"
	rmdir "$(DESTDIR)$(DATADIR)" 2>/dev/null || true


info:
	@echo "TARGET:       $(TARGET)"
	@echo "CC:           $(CC)"
	@echo "CFLAGS:       $(CFLAGS)"
	@echo "LDFLAGS:      $(LDFLAGS)"
	@echo "SRC_FILES:    $(SRC_FILES)"
	@echo "OBJ_FILES:    $(OBJ_FILES)"
	@echo "SINGLE_FILE:  $(SINGLE_FILE)"

.PHONY: all release debug gdb asan test amalgamate single clean install install-single uninstall info
