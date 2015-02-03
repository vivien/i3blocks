/*
 * ini.c - parsing of the INI configuration file
 * Copyright (C) 2014  Vivien Didelot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bar.h"
#include "block.h"
#include "log.h"

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

static struct block *
add_block(struct bar *bar)
{
	struct block *block = NULL;
	void *reloc;

	reloc = realloc(bar->blocks, sizeof(struct block) * (bar->num + 1));
	if (reloc) {
		bar->blocks = reloc;
		block = bar->blocks + bar->num;
		bar->num++;
	}

	return block;
}

static int
parse_section(const char *line, char *name, unsigned int size)
{
	char *closing = strchr(line, ']');
	const int len = strlen(line);

	/* stop if the last char is not a closing bracket */
	if (!closing || line + len - 1 != closing) {
		error("malformated section \"%s\"", line);
		return 1;
	}

	if (size - 1 < len - 2) {
		error("section name too long \"%s\"", line);
		return 1;
	}

	memcpy(name, line + 1, len - 2);
	name[len - 2] = '\0';
	return 0;
}

static int
parse_property(const char *line, struct properties *props, bool strict)
{
	char *equal = strchr(line, '=');
	const char *key, *value;

	if (!equal) {
		error("malformated property, should be a key=value pair");
		return 1;
	}

	/* split key and value */
	*equal = '\0';
	key = line;
	value = equal + 1;

#define PARSE(_name, _size, _flags) \
	if ((!strict || (_flags) & PROP_I3BAR) && strcmp(key, #_name) == 0) { \
		strncpy(props->_name, value, _size - 1); \
		return 0; \
	}

	PROPERTIES(PARSE);

#undef PARSE

	error("unknown key: \"%s\"", key);
	return 1;
}

static int
parse_bar(FILE *fp, struct bar *bar)
{
	char line[2048];
	struct block *block = NULL;
	struct block global = {};

	while (fgets(line, sizeof(line), fp) != NULL) {
		int len = strlen(line);

		if (line[len - 1] != '\n') {
			error("line \"%s\" is not terminated by a newline", line);
			return 1;
		}
		line[len - 1] = '\0';

		switch (*line) {
		/* Comment or empty line? */
		case '#':
		case '\0':
			break;

		/* Section? */
		case '[':
			/* Finalize previous block */
			if (block)
				block_setup(block);

			block = add_block(bar);
			if (!block)
				return 1;

			/* Init the block with default settings (if any) */
			memcpy(block, &global, sizeof(struct block));

			if (parse_section(line, block->default_props.name, sizeof(block->default_props.name)))
				return 1;

			bdebug(block, "new block");
			break;

		/* Property? */
		case 'a' ... 'z':
			if (!block) {
				debug("parsing global properties");
				block = &global;
			}

			if (parse_property(line, &block->default_props, false))
				return 1;

			break;

		/* Syntax error */
		default:
			error("malformated line: %s", line);
			return 1;
		}
	}

	/* Finalize the last block */
	if (block)
		block_setup(block);

	return 0;
}

struct bar *
ini_load(const char *inifile)
{
	const char * const home = getenv("HOME");
	const char * const xdg_home = getenv("XDG_CONFIG_HOME");
	const char * const xdg_dirs = getenv("XDG_CONFIG_DIRS");
	char buf[PATH_MAX];
	FILE *fp;
	struct bar *bar;

	struct bar *parse(void) {
		bar = calloc(1, sizeof(struct bar));
		if (bar && parse_bar(fp, bar)) {
			free(bar->blocks);
			free(bar);
			bar = NULL;
		}

		if (fclose(fp))
			errorx("fclose");

		return bar;
	}

	/* command line config file? */
	if (inifile) {
		debug("try custom config %s", inifile);
		fp = fopen(inifile, "r");
		if (!fp) {
			errorx("fopen");
			return NULL;
		}
		return parse();
	}

	/* user config file? */
	if (home) {
		if (xdg_home)
			snprintf(buf, PATH_MAX, "%s/i3blocks/config", xdg_home);
		else
			snprintf(buf, PATH_MAX, "%s/.config/i3blocks/config", home);
		debug("try XDG home config %s", buf);
		fp = fopen(buf, "r");
		if (fp)
			return parse();

		snprintf(buf, PATH_MAX, "%s/.i3blocks.conf", home);
		debug("try default $HOME config %s", buf);
		fp = fopen(buf, "r");
		if (fp)
			return parse();

		/* if user files don't exist, fall through... */
		if (errno != ENOENT) {
			errorx("fopen");
			return NULL;
		}

		debug("no config found in $HOME");
	}

	/* system config file? */
	if (xdg_dirs)
		snprintf(buf, PATH_MAX, "%s/i3blocks/config", xdg_dirs);
	else
		snprintf(buf, PATH_MAX, "%s/xdg/i3blocks/config", SYSCONFDIR);
	debug("try XDG dirs config %s", buf);
	fp = fopen(buf, "r");
	if (fp)
		return parse();

	snprintf(buf, PATH_MAX, "%s/i3blocks.conf", SYSCONFDIR);
	debug("try default system config %s", buf);
	fp = fopen(buf, "r");
	if (!fp) {
		errorx("fopen");
		return NULL;
	}

	return parse();
}
