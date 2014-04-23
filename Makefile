ifndef PREFIX
  PREFIX=/usr/local
endif
ifndef SYSCONFDIR
  ifeq ($(PREFIX),/usr)
    SYSCONFDIR=/etc
  else
    SYSCONFDIR=$(PREFIX)/etc
  endif
endif

PROGRAM = i3blocks
VERSION = "$(shell git describe --tags --always)"
BLOCK_LIBEXEC = "$(PREFIX)/libexec/$(PROGRAM)"

CPPFLAGS += -DSYSCONFDIR=\"$(SYSCONFDIR)\"
CPPFLAGS += -DVERSION=\"${VERSION}\"
CPPFLAGS += -DBLOCK_LIBEXEC=\"${BLOCK_LIBEXEC}\"
CFLAGS += -Wall

OBJS := $(wildcard src/*.c *.c)
OBJS := $(OBJS:.c=.o)

%.o: %.c %.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

all: $(PROGRAM) man

debug: CPPFLAGS += -DDEBUG
debug: CFLAGS += -g
debug: $(PROGRAM)

$(PROGRAM): ${OBJS}
	$(CC) $(LDFLAGS) -o $@ $^
	@echo " LD $@"

man: $(PROGRAM).1

$(PROGRAM).1: $(PROGRAM).1.ronn
	ronn -w -r $<

clean:
	rm -f *.o $(PROGRAM)

install: all
	install -m 755 -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 -d $(DESTDIR)$(SYSCONFDIR)
	install -m 755 -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 -d $(DESTDIR)$(BLOCK_LIBEXEC)
	install -m 755 $(PROGRAM) $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)
	install -m 644 $(PROGRAM).conf $(DESTDIR)$(SYSCONFDIR)/$(PROGRAM).conf
	install -m 644 $(PROGRAM).1 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 scripts/* $(DESTDIR)$(BLOCK_LIBEXEC)/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)
	rm -f $(DESTDIR)$(SYSCONFDIR)/$(PROGRAM).conf
	rm -f $(DESTDIR)$(SYSCONFDIR)/share/man/man1/$(PROGRAM).1
	rm -rf $(DESTDIR)$(PREFIX)/libexec/$(PROGRAM)

.PHONY: all clean install uninstall
