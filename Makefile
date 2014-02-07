ifndef PREFIX
  PREFIX=/usr
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

VERSION := $(shell git describe --tags --abbrev=0)
GIT_VERSION := "$(shell git describe --tags --always) ($(shell git log --pretty=format:%cd --date=short -n1))"

all: i3blocks

clean:
	rm -f i3blocks

install: all
	install -m 755 -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 -d $(DESTDIR)$(SYSCONFDIR)
	install -m 755 i3blocks $(DESTDIR)$(PREFIX)/bin/i3blocks
	install -m 644 i3blocks.conf $(DESTDIR)$(SYSCONFDIR)/i3blocks.conf

.PHONY: all clean install
