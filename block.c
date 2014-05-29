/*
 * block.c - update of a single status line block
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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

#include "block.h"
#include "log.h"

static int
setup_env(struct block *block)
{
	if (setenv("BLOCK_NAME", block->name, 1) == -1) {
		berrorx(block, "setenv BLOCK_NAME");
		return 1;
	}

	if (setenv("BLOCK_INSTANCE", block->instance, 1) == -1) {
		berrorx(block, "setenv BLOCK_INSTANCE");
		return 1;
	}

	if (setenv("BLOCK_BUTTON", block->click.button, 1) == -1) {
		berrorx(block, "setenv BLOCK_BUTTON");
		return 1;
	}

	if (setenv("BLOCK_X", block->click.x, 1) == -1) {
		berrorx(block, "setenv BLOCK_X");
		return 1;
	}

	if (setenv("BLOCK_Y", block->click.y, 1) == -1) {
		berrorx(block, "setenv BLOCK_Y");
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
mark_as_failed(struct block *block, const char *reason, int status)
{
	static const size_t short_size = sizeof(block->short_text);
	static const size_t full_size = sizeof(block->full_text);
	char short_text[short_size];
	char full_text[full_size];

	if (status < 0)
		snprintf(short_text, short_size, "[%s] ERROR", block->name);
	else
		snprintf(short_text, short_size, "[%s] ERROR (exit:%d)", block->name, status);

	if (*reason)
		snprintf(full_text, full_size, "%s %s", short_text, reason);
	else
		snprintf(full_text, full_size, "%s", short_text);

	strncpy(block->full_text, full_text, full_size);
	strncpy(block->min_width, full_text, sizeof(block->min_width) - 1);
	strncpy(block->short_text, short_text, short_size);
	strcpy(block->color, "#FF0000");
	strcpy(block->urgent, "true");
}

static void
block_reset(struct block *block)
{
	struct click click;
	const struct block *config_block = block->config_block;

	memcpy(&click, &block->click, sizeof(struct click));
	memcpy(block, config_block, sizeof(struct block));
	memcpy(&block->click, &click, sizeof(struct click));

	block->config_block = config_block;
}

void
block_update_command(struct block *block, char *command)
{
	FILE *child_stdout;
	int child_status, code;
	char output[2048], *text = output;

	if (setup_env(block))
		return mark_as_failed(block, "failed to setup env", -1);

	/* Pipe, fork and exec a shell for the block command line */
	child_stdout = popen(command, "r");
	if (!child_stdout) {
		berrorx(block, "popen(%s)", command);
		return mark_as_failed(block, "failed to fork", -1);
	}

	/* Do not distinguish EOF or error, just read child's output */
	memset(output, 0, sizeof(output));
	fread(output, 1, sizeof(output) - 1, child_stdout);

	/* Wait for the child process to terminate */
	child_status = pclose(child_stdout);

	/* Reset the block before update */
	block_reset(block);

	if (child_status == -1) {
		berrorx(block, "pclose");
		return mark_as_failed(block, "failed to wait", -1);
	}

	if (!WIFEXITED(child_status)) {
		berror(block, "child did not exit correctly");
		return mark_as_failed(block, "command did not exit", -1);
	}

	code = WEXITSTATUS(child_status);
	if (code != 0 && code != '!') {
		char reason[1024] = { 0 };

		berror(block, "bad exit code %d", code);
		linecpy(&text, reason, sizeof(reason) - 1);
		return mark_as_failed(block, reason, code);
	}

	/* From here, the update went ok so merge the output */
	strncpy(block->urgent, code == '!' ? "true" : "false", sizeof(block->urgent) - 1);
	linecpy(&text, block->full_text, sizeof(block->full_text) - 1);
	linecpy(&text, block->short_text, sizeof(block->short_text) - 1);
	linecpy(&text, block->color, sizeof(block->color) - 1);
	block->last_update = time(NULL);

	// Reset click
	memset(&block->click, 0, sizeof(struct click));

	bdebug(block, "updated successfully");
}

void
block_update(struct block *block)
{
	block_update_command(block, block->command);
}

void
block_update_wait(struct block *block)
{
	block_update_command(block, block->wait_command);
}
