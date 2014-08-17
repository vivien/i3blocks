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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "block.h"
#include "click.h"
#include "log.h"

static void
child_setup_env(struct block *block, struct click *click)
{
	if (setenv("BLOCK_NAME", NAME(block), 1) == -1)
		_exit(1);

	if (setenv("BLOCK_INSTANCE", INSTANCE(block), 1) == -1)
		_exit(1);

	if (setenv("BLOCK_BUTTON", click ? click->button : "", 1) == -1)
		_exit(1);

	if (setenv("BLOCK_X", click ? click->x : "", 1) == -1)
		_exit(1);

	if (setenv("BLOCK_Y", click ? click->y : "", 1) == -1)
		_exit(1);
}

static void
child_reset_signals(void)
{
	sigset_t set;

	if (sigfillset(&set) == -1)
		_exit(1);

	/* It should be safe to assume that all signals are unblocked by default */
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1)
		_exit(1);
}

static void
child_redirect_write(int pipe[2], int fd)
{
	if (close(pipe[0]) == -1)
		_exit(1);

	/* Defensive check */
	if (pipe[1] == fd)
		return;

	if (dup2(pipe[1], fd) == -1)
		_exit(1);

	if (close(pipe[1]) == -1)
		_exit(1);
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
	struct properties *props = &block->updated_props;

	static const size_t short_size = sizeof(props->short_text);
	static const size_t full_size = sizeof(props->full_text);

	char short_text[short_size];
	char full_text[full_size];

	memset(props, 0, sizeof(struct properties));

	if (status)
		snprintf(short_text, short_size, "[%s] ERROR (exit:%d)", props->name, status);
	else
		snprintf(short_text, short_size, "[%s] ERROR", props->name);

	if (*reason)
		snprintf(full_text, full_size, "%s %s", short_text, reason);
	else
		snprintf(full_text, full_size, "%s", short_text);

	strncpy(props->full_text, full_text, full_size);
	strncpy(props->min_width, full_text, sizeof(props->min_width) - 1);
	strncpy(props->short_text, short_text, short_size);
	strcpy(props->color, "#FF0000");
	strcpy(props->urgent, "true");
}

void
block_spawn(struct block *block, struct click *click)
{
	const unsigned long now = time(NULL);
	int out[2];

	if (!*COMMAND(block)) {
		bdebug(block, "no command, skipping");
		return;
	}

	if (block->pid > 0) {
		bdebug(block, "process already spawned");
		return;
	}

	if (pipe(out) == -1) {
		berrorx(block, "pipe");
		return mark_as_failed(block, "failed to pipe", 0);
	}

	block->pid = fork();
	if (block->pid == -1) {
		berrorx(block, "fork");
		return mark_as_failed(block, "failed to fork", 0);
	}

	/* Child? */
	if (block->pid == 0) {
		child_setup_env(block, click);
		child_reset_signals();
		child_redirect_write(out, STDOUT_FILENO);
		execl("/bin/sh", "/bin/sh", "-c", COMMAND(block), (char *) NULL);
		_exit(1); /* Unlikely */
	}

	/*
	 * Note: no need to set the pipe read end as non-blocking, since it is
	 * meant to be read once the child has exited (and thus the write end is
	 * closed and read is available).
	 */

	/* Parent */
	if (close(out[1]) == -1)
		berrorx(block, "close stdout write end");
	block->pipe = out[0];
	if (!click)
		block->timestamp = now;
	bdebug(block, "forked child %d at %ld", block->pid, now);
}

void
block_reap(struct block *block)
{
	struct properties *props = &block->updated_props;

	char output[2048] = { 0 };
	char *text = output;
	int status, code;

	if (block->pid <= 0) {
		bdebug(block, "not spawned yet");
		return;
	}

	if (waitpid(block->pid, &status, 0) == -1) {
		berrorx(block, "waitpid(%d)", block->pid);
		return;
	}

	code = WEXITSTATUS(status);
	bdebug(block, "process %d exited with %d", block->pid, code);
	block->pid = 0;

	/* Note read(2) returns 0 for end-of-pipe */
	if (read(block->pipe, output, sizeof(output)) == -1) {
		berrorx(block, "read");
		return mark_as_failed(block, "failed to read pipe", 0);
	}

	if (close(block->pipe) == -1) {
		berror(block, "failed to close");
		return mark_as_failed(block, "failed to close read pipe", 0);
	}

	if (code != 0 && code != '!') {
		char reason[1024] = { 0 };

		berror(block, "bad exit code %d", code);
		linecpy(&text, reason, sizeof(reason) - 1);
		return mark_as_failed(block, reason, code);
	}

	/* The update went ok, so reset the defaults and merge the output */
	memcpy(props, &block->default_props, sizeof(struct properties));
	strncpy(props->urgent, code == '!' ? "true" : "false", sizeof(props->urgent) - 1);
	linecpy(&text, props->full_text, sizeof(props->full_text) - 1);
	linecpy(&text, props->short_text, sizeof(props->short_text) - 1);
	linecpy(&text, props->color, sizeof(props->color) - 1);
	bdebug(block, "updated successfully");
}

void block_setup(struct block *block)
{
	struct properties *defaults = &block->default_props;
	struct properties *updated = &block->updated_props;

	/* Convenient shortcuts */
	block->interval = atoi(defaults->interval);
	block->signal = atoi(defaults->signal);

	/* First update (for static blocks and loading labels) */
	memcpy(updated, defaults, sizeof(struct properties));

#define PLACEHOLDERS(_name, _size, _flags) "    %s: \"%s\"\n"
#define ARGS(_name, _size, _flags) #_name, updated->_name,

	debug("\n{\n" PROPERTIES(PLACEHOLDERS) "%s", PROPERTIES(ARGS) "}");

#undef ARGS
#undef PLACEHOLDERS
}
