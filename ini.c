/*
 * i3blocks - define blocks for your i3bar status line
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "log.h"

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

static struct block *
add_block(struct status_line *status)
{
	struct block *block = NULL;
	void *reloc;

	reloc = realloc(status->blocks, sizeof(struct block) * (status->num + 1));
	if (reloc) {
		status->blocks = reloc;
		block = status->blocks + status->num;
		status->num++;
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
	name[len - 1] = '\0';
	return 0;
}

static int
parse_property(const char *line, struct block *block)
{
	char *equal = strchr(line, '=');
	const char *property, *value;

	if (!equal) {
		error("malformated property, should be of the form 'key=value'");
		return 1;
	}

	/* split property and value */
	*equal = '\0';
	property = line;
	value = equal + 1;

#define PARSE(_name, _size, _type) \
	if (strcmp(property, #_name) == 0) { \
		strncpy(block->_name, value, _size - 1); \
		goto parsed; \
	} \

#define PARSE_NUM(_name) \
	if (strcmp(property, #_name) == 0) { \
		block->_name = atoi(value); \
		goto parsed; \
	} \

	PROTOCOL_KEYS(PARSE);
	PARSE(command, sizeof(block->command), _);
	PARSE_NUM(interval);
	PARSE_NUM(signal);
	/* TODO some better check for numbers and boolean */

#undef PARSE_NUM
#undef PARSE

	error("unknown property: \"%s\"", property);
	return 1;

parsed:
	debug("[%s] set property %s to \"%s\"", block->name, property, value);
	return 0;
}

static int
duplicate_blocks(struct status_line *status)
{
	const size_t size = status->num * sizeof(struct block);

	status->updated_blocks = malloc(size);
	if (!status->updated_blocks)
		return 1;

	memcpy(status->updated_blocks, status->blocks, size);
	return 0;
}

static int
parse_status_line(FILE *fp, struct status_line *status)
{
	struct block *block = NULL;
	struct block global;
	char line[2048];

	memset(&global, 0, sizeof(struct block));

	while (fgets(line, sizeof(line), fp) != NULL) {
		int len = strlen(line);

		if (line[len - 1] != '\n') {
			error("line \"%s\" is not terminated by a newline", line);
			return 1;
		}
		line[len - 1] = '\0';
		debug("parsing line: %s", line);

		switch (*line) {
		/* Comment or empty line? */
		case '#':
		case '\0':
			break;

		/* Section? */
		case '[':
			block = add_block(status);
			if (!block)
				return 1;

			/* Init the block with default settings (if any) */
			memcpy(block, &global, sizeof(struct block));

			if (parse_section(line, block->name, sizeof(block->name)))
				return 1;

			debug("[%s] new block", block->name);
			break;

		/* Property? */
		case 'a' ... 'z':
			if (!block) {
				debug("no section yet, consider global properties");
				block = &global;
			}

			if (parse_property(line, block))
				return 1;

			break;

		/* Syntax error */
		default:
			error("malformated line: %s", line);
			return 1;
		}
	}

	return duplicate_blocks(status);
}

struct status_line *
ini_load_status_line(const char *inifile)
{
	static const char * const system = SYSCONFDIR "/i3blocks.conf";
	const char * const home = getenv("HOME");
	char buf[PATH_MAX];
	FILE *fp;
	struct status_line *status;

	struct status_line *parse(void) {
		status = calloc(1, sizeof(struct status_line));
		if (status && parse_status_line(fp, status)) {
			free(status->blocks);
			free(status->updated_blocks);
			free(status);
			status = NULL;
		}

		if (fclose(fp))
			errorx("fclose");

		return status;
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
		snprintf(buf, PATH_MAX, "%s/.i3blocks.conf", home);
		debug("try $HOME config %s", buf);
		fp = fopen(buf, "r");
		if (fp)
			return parse();

		/* if the file doesn't exist, fall through... */
		if (errno != ENOENT) {
			errorx("fopen");
			return NULL;
		}

		debug("no config found in $HOME");
	}

	/* system config file? */
	debug("try system config %s", system);
	fp = fopen(system, "r");
	if (!fp) {
		errorx("fopen");
		return NULL;
	}

	return parse();
}
