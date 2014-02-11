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

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "status_line.h"

static void
calculate_sleeptime(struct status_line *status)
{
	int time = 0;

	/* The maximum sleep time is actually the GCD between all block intervals */
	int gcd(int a, int b) {
		while (b != 0)
			a %= b, a ^= b, b ^= a, a ^= b;

		return a;
	}

	if (status->num > 0) {
		time = status->blocks->interval; /* first block's interval */

		if (status->num >= 2) {
			int i;

			for (i = 1; i < status->num; ++i)
				time = gcd(time, (status->blocks + i)->interval);
		}
	}

	status->sleeptime = time > 0 ? time : 5; /* default */
}

static struct block *
new_block(struct status_line *status)
{
	struct block *block = NULL;
	void *reloc;

	reloc = realloc(status->blocks, sizeof(struct block) * (status->num + 1));
	if (reloc) {
		status->blocks = reloc;
		block = status->blocks + status->num;
		init_block(block);
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
	char *property;

	if (!equal) {
		fprintf(stderr, "malformated property, should be of the form 'key=value'\n");
		return 1;
	}

	property = equal + 1;

	if (strncmp(line, "command", sizeof("command") - 1) == 0) {
		block->command = strdup(property);
		if (!block->command) return 1;
		return 0;
	}

	if (strncmp(line, "interval", sizeof("interval") - 1) == 0) {
		block->interval = atoi(property); /* FIXME not rigorous */
		return 0;
	}

	if (strncmp(line, "full_text", sizeof("full_text") - 1) == 0) {
		block->full_text = strdup(property);
		if (!block->full_text) return 1;
		return 0;
	}

	if (strncmp(line, "short_text", sizeof("short_text") - 1) == 0) {
		block->short_text = strdup(property);
		if (!block->short_text) return 1;
		return 0;
	}

	if (strncmp(line, "color", sizeof("color") - 1) == 0) {
		block->color = strdup(property);
		if (!block->color) return 1;
		return 0;
	}

	if (strncmp(line, "min_width", sizeof("min_width") - 1) == 0) {
		block->min_width = strdup(property);
		if (!block->min_width) return 1; // FIXME may be a string or a int, should be parsed on json encoding
		return 0;
	}

	if (strncmp(line, "align", sizeof("align") - 1) == 0) {
		block->align = strdup(property);
		if (!block->align) return 1;
		return 0;
	}

	if (strncmp(line, "name", sizeof("name") - 1) == 0) {
		block->name = strdup(property);
		if (!block->name) return 1;
		return 0;
	}

	if (strncmp(line, "instance", sizeof("instance") - 1) == 0) {
		block->instance = strdup(property);
		if (!block->instance) return 1;
		return 0;
	}

	if (strncmp(line, "urgent", sizeof("urgent") - 1) == 0) {
		block->urgent = strcmp(property, "true") == 0; /* FIXME not rigorous */
		return 0;
	}

	if (strncmp(line, "separator", sizeof("separator") - 1) == 0) {
		block->separator = strcmp(property, "true") == 0; /* FIXME not rigorous */
		return 0;
	}

	if (strncmp(line, "separator_block_width", sizeof("separator_block_width") - 1) == 0) {
		block->separator_block_width = atoi(property); /* FIXME not rigorous */
		return 0;
	}

	printf("unknown property: \"%s\"\n", line);
	return 1;
}

static int
parse_status_line(FILE *fp, struct status_line *status)
{
	struct block *block = status->blocks;
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

			block = new_block(status);
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
				fprintf(stderr, "global properties not supported, need a section\n");
				return 1;
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

void
print_status_line(struct status_line *status)
{
	bool first = true;
	int i = 0;

	fprintf(stdout, ",[");

	for (i = 0; i < status->num; ++i) {
		struct block *block = status->blocks + i;

		/* full_text is the only mandatory key */
		if (!block->full_text)
			continue;

		if (!first)
			fprintf(stdout, ",");
		else
			first = false;

		block_to_json(block);
	}

	fprintf(stdout, "]\n");
	fflush(stdout);
}

void
update_status_line(struct status_line *status)
{
	int i;

	for (i = 0; i < status->num; ++i) {
		struct block *block = status->blocks + i;

		if (need_update(block) && update_block(block))
			fprintf(stderr, "failed to update block %s\n", block->name);
	}
}

static void
free_status_line(struct status_line *status)
{
	int i;

	for (i = 0; i < status->num; ++i)
		free_block(status->blocks + i);

	free(status);
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

void
mark_update(struct status_line *status)
{
	int i;

	for (i = 0; i < status->num; ++i)
		(status->blocks + i)->last_update = 0;
}
