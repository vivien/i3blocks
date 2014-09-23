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
ifndef DOCDIR
  DOCDIR=$(PREFIX)/share/doc/$(PROGRAM)
endif

PROGRAM = i3blocks
VERSION = "$(shell git describe --tags --always)"

CPPFLAGS += -DSYSCONFDIR=\"$(SYSCONFDIR)\"
CPPFLAGS += -DVERSION=\"${VERSION}\"
CFLAGS += -std=gnu99 -Wall -Werror=format-security

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
	install -m 755 -d $(DESTDIR)$(LIBEXECDIR)/$(PROGRAM)
	install -m 755 $(PROGRAM) $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)
	sed 's,$$SCRIPT_DIR,$(LIBEXECDIR)/$(PROGRAM),' $(PROGRAM).conf > $(DESTDIR)$(SYSCONFDIR)/$(PROGRAM).conf
	chmod 644 $(DESTDIR)$(SYSCONFDIR)/$(PROGRAM).conf
	install -m 644 $(PROGRAM).1 $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 755 scripts/* $(DESTDIR)$(LIBEXECDIR)/$(PROGRAM)/
	install -m 755 -d $(DESTDIR)$(DOCDIR)/contrib
	install -m 755 contrib/* $(DESTDIR)$(DOCDIR)/contrib/
	chmod 644 $(DESTDIR)$(DOCDIR)/contrib/config

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(PROGRAM)
	rm -f $(DESTDIR)$(SYSCONFDIR)/$(PROGRAM).conf
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/$(PROGRAM).1
	rm -rf $(DESTDIR)$(LIBEXECDIR)/$(PROGRAM)
	rm -rf $(DESTDIR)$(DOCDIR)

.PHONY: all clean install uninstall
