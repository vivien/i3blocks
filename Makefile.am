DEFS += \
	-DSYSCONFDIR=\"$(sysconfdir)\"

bin_PROGRAMS = i3blocks
i3blocks_SOURCES = \
	bar.c \
	bar.h \
	block.c \
	block.h \
	config.c \
	config.h \
	i3bar.c \
	ini.c \
	ini.h \
	json.c \
	json.h \
	line.c \
	line.h \
	log.h \
	main.c \
	map.c \
	map.h \
	sys.c \
	sys.h \
	term.h

dist_man1_MANS = \
	docs/i3blocks.1

dist_sysconf_DATA = \
	i3blocks.conf

if ENABLE_BASH_COMPLETION
bashcompletiondir = $(BASH_COMPLETION_DIR)
bashcompletion_DATA = bash-completion

install-data-local:
	( cd '$(DESTDIR)$(BASH_COMPLETION_DIR)' && mv bash-completion i3blocks )

uninstall-local:
	( cd '$(DESTDIR)$(BASH_COMPLETION_DIR)' && rm -rf i3blocks )
endif
