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
#include "json.h"
#include "log.h"

static void
child_setenv(struct block *block, const char *name, const char *value)
{
	if (setenv(name, value, 1) == -1) {
		berrorx(block, "setenv(%s=%s)", name, value);
		_exit(EXIT_ERR_INTERNAL);
	}
}

static void
child_setup_env(struct block *block, struct click *click)
{
	child_setenv(block, "BLOCK_NAME", NAME(block));
	child_setenv(block, "BLOCK_INSTANCE", INSTANCE(block));
	child_setenv(block, "BLOCK_BUTTON", click ? click->button : "");
	child_setenv(block, "BLOCK_X", click ? click->x : "");
	child_setenv(block, "BLOCK_Y", click ? click->y : "");
}

static void
child_reset_signals(struct block *block)
{
	sigset_t set;

	if (sigfillset(&set) == -1) {
		berrorx(block, "sigfillset");
		_exit(EXIT_ERR_INTERNAL);
	}

	/* It should be safe to assume that all signals are unblocked by default */
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1) {
		berrorx(block, "sigprocmask");
		_exit(EXIT_ERR_INTERNAL);
	}
}

static void
child_redirect_write(struct block *block, int pipe[2], int fd)
{
	if (close(pipe[0]) == -1) {
		berrorx(block, "close pipe read end");
		_exit(EXIT_ERR_INTERNAL);
	}

	/* Defensive check */
	if (pipe[1] == fd)
		return;

	if (dup2(pipe[1], fd) == -1) {
		berrorx(block, "dup pipe write end");
		_exit(EXIT_ERR_INTERNAL);
	}

	if (close(pipe[1]) == -1) {
		berrorx(block, "close pipe write end");
		_exit(EXIT_ERR_INTERNAL);
	}
}

static void
child_exec(struct block *block)
{
	static const char * const shell = "/bin/sh";

	execl(shell, shell, "-c", COMMAND(block), (char *) NULL);
	/* Unlikely to reach this point */
	berrorx(block, "exec(%s -c %s)", shell, COMMAND(block));
	_exit(EXIT_ERR_INTERNAL);
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
mark_as_failed(struct block *block, const char *reason)
{
	if (log_level < LOG_WARN)
		return;

	struct properties *props = &block->updated_props;

	memset(props, 0, sizeof(struct properties));
	snprintf(props->full_text, sizeof(props->full_text), "[%s] %s", NAME(block), reason);
	snprintf(props->short_text, sizeof(props->short_text), "[%s] ERROR", NAME(block));
	strcpy(props->color, "#FF0000");
	strcpy(props->urgent, "true");
}

void
block_spawn(struct block *block, struct click *click)
{
	const unsigned long now = time(NULL);
	int out[2], err[2];

	if (!*COMMAND(block)) {
		bdebug(block, "no command, skipping");
		return;
	}

	if (block->pid > 0) {
		bdebug(block, "process already spawned");
		return;
	}

	if (pipe(out) == -1 || pipe(err) == -1) {
		berrorx(block, "pipe");
		return mark_as_failed(block, strerror(errno));
	}

	block->pid = fork();
	if (block->pid == -1) {
		berrorx(block, "fork");
		return mark_as_failed(block, strerror(errno));
	}

	/* Child? */
	if (block->pid == 0) {
		/* Error messages are merged into the parent's stderr... */
		child_setup_env(block, click);
		child_reset_signals(block);
		child_redirect_write(block, out, STDOUT_FILENO);
		child_redirect_write(block, err, STDERR_FILENO);
		/* ... until here */
		child_exec(block);
	}

	/*
	 * Note: no need to set the pipe read end as non-blocking, since it is
	 * meant to be read once the child has exited (and thus the write end is
	 * closed and read is available).
	 */

	/* Parent */
	if (close(out[1]) == -1)
		berrorx(block, "close stdout");
	if (close(err[1]) == -1)
		berrorx(block, "close stderr");

	block->out = out[0];
	block->err = err[0];

	if (!click)
		block->timestamp = now;

	bdebug(block, "forked child %d at %ld", block->pid, now);
}

void
block_reap(struct block *block)
{
	struct properties *props = &block->updated_props;

	char buf[2048] = { 0 };
	char *lines = buf;
	int status, code;

	if (block->pid <= 0) {
		bdebug(block, "not spawned yet");
		return;
	}

	if (waitpid(block->pid, &status, 0) == -1) {
		berrorx(block, "waitpid(%d)", block->pid);
		mark_as_failed(block, strerror(errno));
		goto close;
	}

	code = WEXITSTATUS(status);
	bdebug(block, "process %d exited with %d", block->pid, code);

	/* Note read(2) returns 0 for end-of-pipe */
	if (read(block->err, buf, sizeof(buf)) == -1) {
		berrorx(block, "read stderr");
		mark_as_failed(block, strerror(errno));
		goto close;
	}

	if (*buf) {
		bdebug(block, "stderr:\n{\n%s\n}", buf);
		memset(buf, 0, sizeof(buf));
	}

	if (code != 0 && code != EXIT_URGENT) {
		char reason[32];

		if (code == EXIT_ERR_INTERNAL)
			sprintf(reason, "internal error");
		else
			sprintf(reason, "bad exit code %d", code);

		berror(block, "%s", reason);
		mark_as_failed(block, reason);
		goto close;
	}

	/* Note read(2) returns 0 for end-of-pipe */
	if (read(block->out, buf, sizeof(buf)) == -1) {
		berrorx(block, "read stdout");
		mark_as_failed(block, strerror(errno));
		goto close;
	}

	/* The update went ok, so reset the defaults and merge the output */
	memcpy(props, &block->default_props, sizeof(struct properties));
	strncpy(props->urgent, code == EXIT_URGENT ? "true" : "false", sizeof(props->urgent) - 1);

	if (block->output == OUTPUT_PLAIN) {
		/* PLAIN parsing: read three lines for full, short and color */
		linecpy(&lines, props->full_text, sizeof(props->full_text) - 1);
		linecpy(&lines, props->short_text, sizeof(props->short_text) - 1);
		linecpy(&lines, props->color, sizeof(props->color) - 1);

	} else {
		/* JSON parsing: parse object containing PROPERTIES pairs */
		int start, length;

#define MIN(a, b) (a < b ? a : b)
#define JSONPARSE(_name, _size, _flags) \
	if ((_flags) & PROP_I3BAR) { \
		json_parse(buf, #_name, &start, &length); \
		if (start > 0) { \
			strncpy(props->_name, buf + start, MIN(_size, length)); \
			props->_name[length] = '\0'; \
		} \
	}

		PROPERTIES(JSONPARSE);

#undef JSONPARSE
#undef MIN

	}

	if (*FULL_TEXT(block) && *LABEL(block)) {
		static const size_t size = sizeof(props->full_text);
		char concat[size];
		snprintf(concat, size, "%s %s", LABEL(block), FULL_TEXT(block));
		strcpy(props->full_text, concat);
	}

	bdebug(block, "updated successfully");
close:
	if (close(block->out) == -1)
		berrorx(block, "close stdout");
	if (close(block->err) == -1)
		berrorx(block, "close stderr");

	block->pid = 0;
}

void block_setup(struct block *block)
{
	struct properties *defaults = &block->default_props;
	struct properties *updated = &block->updated_props;

	/* Convenient shortcuts */
	if (strcmp(defaults->interval, "once") == 0)
		block->interval = INTER_ONCE;
	else if (strcmp(defaults->interval, "repeat") == 0)
		block->interval = INTER_REPEAT;
	else
		block->interval = atoi(defaults->interval);
	block->signal = atoi(defaults->signal);

	block->output = OUTPUT_PLAIN;
	if (strcmp(defaults->output, "json") == 0)
		block->output = OUTPUT_JSON;

	/* First update (for static blocks and loading labels) */
	memcpy(updated, defaults, sizeof(struct properties));

#define PLACEHOLDERS(_name, _size, _flags) "    %s: \"%s\"\n"
#define ARGS(_name, _size, _flags) #_name, updated->_name,

	debug("\n{\n" PROPERTIES(PLACEHOLDERS) "%s", PROPERTIES(ARGS) "}");

#undef ARGS
#undef PLACEHOLDERS
}
