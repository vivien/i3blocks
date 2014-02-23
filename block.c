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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#include "block.h"

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

static void
linecpy(char **lines, char *dest, unsigned int size)
{
	char *newline = strchr(*lines, '\n');

	/* split if there's a newline */
	if (newline)
		*newline = '\0';

	/* if text in non-empty, copy it */
	if (**lines) {
		strncpy(dest, *lines, size);
		*lines += strlen(dest);
	}

	/* increment if next char is non-null */
	if (*(*lines + 1))
		*lines += 1;
}

static void
failed(const char *reason, struct block *block)
{
	strncpy(block->full_text, reason, sizeof(block->full_text) - 1);
	strncpy(block->min_width, reason, sizeof(block->min_width) - 1);
	strcpy(block->short_text, "FAILED");
	strcpy(block->color, "#FF0000");
	strcpy(block->urgent, "true");
}

static int
update_block(struct block *block)
{
	FILE *child_stdout;
	int child_status, code;
	char output[2048], *text = output;

	if (setup_env(block))
		return 1;

	/* Pipe, fork and exec a shell for the block command line */
	child_stdout = popen(block->command, "r");
	if (!child_stdout) {
		perror("popen");
		return 1;
	}

	/* Do not distinguish EOF or error, just read child's output */
	memset(output, 0, sizeof(output));
	fread(output, 1, sizeof(output) - 1, child_stdout);

	/* Wait for the child process to terminate */
	child_status = pclose(child_stdout);
	if (child_status == -1) {
		perror("pclose");
		return 1;
	}

	if (!WIFEXITED(child_status)) {
		fprintf(stderr, "child did not exit correctly\n");
		return 1;
	}

	code = WEXITSTATUS(child_status);
	if (code != 0 && code != 127) {
		char reason[1024];

		fprintf(stderr, "bad return code %d, skipping %s\n", code, block->name);
		sprintf(reason, "[%s] ERROR: bad return code %d", block->name, code);
		failed(reason, block);
		return 1;
	}

	/* From here, the update went ok so merge the output */
	strncpy(block->urgent, code == 127 ? "true" : "false", sizeof(block->urgent) - 1);
	linecpy(&text, block->full_text, sizeof(block->full_text) - 1);
	linecpy(&text, block->short_text, sizeof(block->short_text) - 1);
	linecpy(&text, block->color, sizeof(block->color) - 1);
	block->last_update = time(NULL);

	return 0;
}

static inline int
need_update(struct block *block)
{
	const unsigned long now = time(NULL);
	const unsigned long next_update = block->last_update + block->interval;

	return ((long) (next_update - now)) <= 0;
}

void
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

void
update_status_line(struct status_line *status)
{
	int i;

	for (i = 0; i < status->num; ++i) {
		const struct block *config_block = status->blocks + i;
		struct block *updated_block = status->updated_blocks + i;

		/* Skip static block */
		if (!*config_block->command)
			continue;

		/* If a block needs an update, reset and execute it */
		if (need_update(updated_block)) {
			memcpy(updated_block, config_block, sizeof(struct block));
			if (update_block(updated_block))
				fprintf(stderr, "failed to update block %s\n", updated_block->name);
		}
	}
}

void
free_status_line(struct status_line *status)
{
	int i;

	for (i = 0; i < status->num; ++i)
		free(status->blocks + i);

	free(status);
}

void
mark_update(struct status_line *status)
{
	int i;

	for (i = 0; i < status->num; ++i)
		(status->updated_blocks + i)->last_update = 0;
}
