/*
 * i3blocks - simple i3 status line
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
		memset(block, 0, sizeof(struct block));
		status->num++;
	}

	return block;
}

static char *
parse_section(const char *line)
{
	char *closing = strchr(line, ']');
	int len = strlen(line);

	/* stop if the last char is not a closing bracket */
	if (!closing || line + len - 1 != closing) {
		fprintf(stderr, "malformated section \"%s\"\n", line);
		return NULL;
	}

	return strndup(line + 1, len - 2);
}

static int
parse_property(const char *line, struct block *block)
{
	char *equal = strchr(line, '=');
	const char *property, *value;

	if (!equal) {
		fprintf(stderr, "malformated property, should be of the form 'key=value'\n");
		return 1;
	}

	/* split property and value */
	*equal = '\0';
	property = line;
	value = equal + 1;

#define PARSE(_name, _) \
	if (strcmp(property, #_name) == 0) { \
		block->_name = strdup(value); \
		return block->_name == NULL; \
	} \

#define PARSE_NUM(_name) \
	if (strcmp(property, #_name) == 0) { \
		block->_name = atoi(value); \
		return 0; \
	} \

	PROTOCOL_KEYS(PARSE);
	PARSE(command, _);
	PARSE_NUM(interval);
	/* TODO some better check for numbers and boolean */

	printf("unknown property: \"%s\"\n", property);
	return 1;
}

static int
parse_status_line(FILE *fp, struct status_line *status)
{
	struct block *block = NULL;
	char line[1024];
	char *name;

	while (fgets(line, sizeof(line), fp) != NULL) {
		int len = strlen(line);

		if (line[len - 1] != '\n') {
			fprintf(stderr, "line \"%s\" is not terminated by a newline\n", line);
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
			name = parse_section(line);
			if (!name)
				return 1;

			block = add_block(status);
			if (!block) {
				free(name);
				return 1;
			}

			block->name = name;
			/* fprintf(stderr, "new block named: \"%s\"\n", block->name); */
			break;

		/* Property? */
		case 'a' ... 'z':
			if (!block) {
				fprintf(stderr, "no section yet, creating global properties\n");
				status->global = calloc(1, sizeof(struct block));
				if (!status->global)
					return 1;
				block = status->global;
			}

			if (parse_property(line, block))
				return 1;

			break;

		/* Syntax error */
		default:
			fprintf(stderr, "malformated line \"%s\"\n", line);
			return 1;
		}
	}

	calculate_sleeptime(status);
	return 0;
}

struct status_line *
load_status_line(const char *inifile)
{
	static const char * const system = SYSCONFDIR "/i3blocks.conf";
	const char * const home = getenv("HOME");
	char buf[PATH_MAX];
	FILE *fp;
	struct status_line *status;

	struct status_line *parse(void) {
		status = calloc(1, sizeof(struct status_line));
		if (status && parse_status_line(fp, status)) {
			free_status_line(status);
			status = NULL;
		}

		if (fclose(fp))
			perror("fclose");

		return status;
	}

	/* command line config file? */
	if (inifile) {
		fp = fopen(inifile, "r");
		if (!fp) {
			perror("fopen");
			return NULL;
		}

		return parse();
	}

	/* user config file? */
	if (home) {
		snprintf(buf, PATH_MAX, "%s/.i3blocks.conf", home);
		fp = fopen(buf, "r");
		if (fp)
			return parse();

		/* if the file doesn't exist, fall through... */
		if (errno != ENOENT) {
			perror("fopen");
			return NULL;
		}
	}

	/* system config file? */
	fp = fopen(system, "r");
	if (!fp) {
		perror("fopen");
		return NULL;
	}

	return parse();
}
