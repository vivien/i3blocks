RELEASE_VERSION = 1.3

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
ifndef LIBEXECDIR
  LIBEXECDIR=$(PREFIX)/libexec
endif
ifndef VERSION
  VERSION = $(shell git describe --tags --always 2> /dev/null)
  ifeq ($(strip $(VERSION)),)
    VERSION = $(RELEASE_VERSION)
  endif
endif

PROGRAM = i3blocks

CPPFLAGS += -DSYSCONFDIR=\"$(SYSCONFDIR)\"
CPPFLAGS += -DVERSION=\"${VERSION}\"
CFLAGS += -std=gnu99 -Iinclude -Wall -Werror=format-security

OBJS := $(wildcard src/*.c)
OBJS := $(OBJS:.c=.o)

%.o: %.c %.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
	@echo " CC $<"

all: $(PROGRAM)

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
	rm -f src/*.o $(PROGRAM)

install: all man
	install -m 755 -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 -d $(DESTDIR)$(SYSCONFDIR)
	install -m 755 -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 -d $(DESTDIR)$(LIBEXECDIR)/$(PROGRAM)
	install -m 755 $(PROGRAM) $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)
	sed 's,$$SCRIPT_DIR,$(LIBEXECDIR)/$(PROGRAM),' $(PROGRAM).conf > $(DESTDIR)$(SYSCONFDIR)/$(PROGRAM).conf
	chmod 644 $(DESTDIR)$(SYSCONFDIR)/$(PROGRAM).conf
	install -m 644 $(PROGRAM).1 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 scripts/* $(DESTDIR)$(LIBEXECDIR)/$(PROGRAM)/

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)
	rm -f $(DESTDIR)$(SYSCONFDIR)/$(PROGRAM).conf
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/$(PROGRAM).1
	rm -rf $(DESTDIR)$(LIBEXECDIR)/$(PROGRAM)

dist: clean man
	( git ls-files * ; ls $(PROGRAM).1 ) | \
	  tar -czT - --transform 's,^,$(PROGRAM)-$(VERSION)/,' \
	  -f $(PROGRAM)-$(VERSION).tar.gz

distclean: clean
	rm -f $(PROGRAM).1 $(PROGRAM)-*.tar.gz

.PHONY: all clean install uninstall
