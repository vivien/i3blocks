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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#include "block.h"

void
init_block(struct block *block)
{
	memset(block, 0, sizeof(struct block));
	block->separator = true;
	block->separator_block_width = 9;
}

void
free_block(struct block *block)
{
	if (block->full_text) free(block->full_text);
	if (block->short_text) free(block->short_text);
	if (block->color) free(block->color);
	if (block->min_width) free(block->min_width);
	if (block->align) free(block->align);
	if (block->name) free(block->name);
	if (block->instance) free(block->instance);
	if (block->command) free(block->command);
	free(block);
}

void
block_to_json(struct block *block)
{
	fprintf(stdout, "{");

	fprintf(stdout, "\"full_text\":\"%s\"", block->full_text);

	if (block->short_text)
		fprintf(stdout, ",\"short_text\":\"%s\"", block->short_text);
	if (block->color)
		fprintf(stdout, ",\"color\":\"%s\"", block->color);
	if (block->min_width)
		fprintf(stdout, ",\"min_width\":\"%s\"", block->min_width);
	if (block->align)
		fprintf(stdout, ",\"align\":\"%s\"", block->align);
	if (block->name)
		fprintf(stdout, ",\"name\":\"%s\"", block->name);
	if (block->instance)
		fprintf(stdout, ",\"instance\":\"%s\"", block->instance);
	if (block->urgent)
		fprintf(stdout, ",\"urgent\":true");
	if (!block->separator)
		fprintf(stdout, ",\"separator\":false");
	if (block->separator_block_width != 9)
		fprintf(stdout, ",\"separator_block_width\":%d", block->separator_block_width);

	fprintf(stdout, "}");

}

static char *
readline(FILE *fp)
{
	char *line = NULL;
	char buf[1024];

	if (fgets(buf, sizeof(buf), fp) != NULL) {
		size_t len = strlen(buf);

		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';

		line = strdup(buf);
	}

	return line;
}

int
update_block(struct block *block)
{
	FILE *child_stdout;
	int child_status, code;
	char *full_text, *short_text, *color;

	/* static block */
	if (!block->command)
		return 0;

	/* TODO setup env */
	child_stdout = popen(block->command, "r");
	if (!child_stdout) {
		perror("popen");
		return 1;
	}

	full_text = readline(child_stdout);
	short_text = readline(child_stdout);
	color = readline(child_stdout);

	child_status = pclose(child_stdout);
	if (child_status == -1) {
		perror("pclose");
		return 1;
	} else if (!WIFEXITED(child_status)) {
		fprintf(stderr, "child did not exit correctly\n");
		return 1;
	} else {
		code = WEXITSTATUS(child_status);

		if (code != 0 && code != 127) {
			fprintf(stderr, "bad return code %d, skipping\n", code);
			return 1;
		}

		block->urgent = code == 127;

		if (full_text && *full_text != '\0') {
			if (block->full_text)
				free(block->full_text);
			block->full_text = full_text;
		}

		if (short_text && *short_text != '\0') {
			if (block->short_text)
				free(block->short_text);
			block->short_text = short_text;
		}

		if (color && *color != '\0') {
			if (block->color)
				free(block->color);
			block->color = color;
		}

		block->last_update = time(NULL);
	}

	return 0;
}

inline int
need_update(struct block *block)
{
	const unsigned long now = time(NULL);
	const unsigned long next_update = block->last_update + block->interval;

	return ((long) (next_update - now)) <= 0;
}
