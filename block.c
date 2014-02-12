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
free_block(struct block *block)
{
#define FREE(_name, _type) \
	if (block->_name) \
		free(block->_name);

	PROTOCOL_KEYS(FREE);
	FREE(command, string);
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

static int
setup_env(struct block *block)
{
	if (setenv("BLOCK_NAME", block->name, 1) == -1) {
		perror("setenv(name)");
		return 1;
	}

	if (setenv("BLOCK_INSTANCE", block->instance, 1) == -1) {
		perror("setenv(instance)");
		return 1;
	}

	return 0;
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

	if (setup_env(block))
		return 1;

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

		if (block->urgent)
			free(block->urgent);
		if (code == 127) {
			block->urgent = strdup("true");
			if (!block->urgent) {
				perror("strdup(urgent)");
				return 1;
			}
		}

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
