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

all: i3blocks

i3blocks: ${OBJS}
	$(CC) $(LDFLAGS) -o $@ $^
	@echo " LD $@"

clean:
	rm -f *.o i3blocks

install: all
	install -m 755 -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 -d $(DESTDIR)$(SYSCONFDIR)
	install -m 755 i3blocks $(DESTDIR)$(PREFIX)/bin/i3blocks
	install -m 644 i3blocks.conf $(DESTDIR)$(SYSCONFDIR)/i3blocks.conf

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/i3blocks
	rm -f $(DESTDIR)$(SYSCONFDIR)/i3blocks.conf

.PHONY: all clean install uninstall
