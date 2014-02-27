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

CFLAGS += -Wall
CFLAGS += -g
CPPFLAGS += -DSYSCONFDIR=\"$(SYSCONFDIR)\"
CPPFLAGS += -DVERSION=\"${GIT_VERSION}\"

VERSION = $(shell git describe --tags --abbrev=0)
GIT_VERSION = "$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1))"

OBJS := $(wildcard src/*.c *.c)
OBJS := $(OBJS:.c=.o)

%.o: %.c %.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

all: i3blocks man

i3blocks: ${OBJS}
	$(CC) $(LDFLAGS) -o $@ $^
	@echo " LD $@"

man: i3blocks.1

i3blocks.1: i3blocks.1.ronn
	ronn -w -r $<

clean:
	rm -f *.o i3blocks i3blocks.1

install: all
	install -m 755 -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 -d $(DESTDIR)$(SYSCONFDIR)
	install -m 755 -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 -d $(DESTDIR)$(PREFIX)/libexec/i3blocks
	install -m 755 i3blocks $(DESTDIR)$(PREFIX)/bin/i3blocks
ifneq ($(DESTDIR)$(PREFIX), /usr)
	sed "s,/usr/,$(DESTDIR)$(PREFIX)/," i3blocks.conf > $(DESTDIR)$(SYSCONFDIR)/i3blocks.conf
	chmod 644 $(DESTDIR)$(SYSCONFDIR)/i3blocks.conf
else
	install -m 644 i3blocks.conf $(DESTDIR)$(SYSCONFDIR)/i3blocks.conf
endif
	install -m 644 i3blocks.1 $(DESTDIR)$(PREFIX)/share/man/man1 || true
	install -m 755 scripts/* $(DESTDIR)$(PREFIX)/libexec/i3blocks/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/i3blocks
	rm -f $(DESTDIR)$(SYSCONFDIR)/i3blocks.conf
	rm -f $(DESTDIR)$(SYSCONFDIR)/share/man/man1/i3blocks.1
	rm -rf $(DESTDIR)$(PREFIX)/libexec/i3blocks

.PHONY: all clean install uninstall
