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

#include <errno.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "block.h"
#include "click.h"
#include "io.h"
#include "json.h"
#include "log.h"

const char *block_get(const struct block *block, const char *key)
{
	return map_get(block->customs, key);
}

static int block_set(struct block *block, const char *key, const char *value)
{
	return map_set(block->customs, key, value);
}

static int block_reset(struct block *block)
{
	map_clear(block->customs);

	return map_copy(block->customs, block->defaults);
}

int block_for_each(const struct block *block,
		   int (*func)(const char *key, const char *value, void *data),
		   void *data)
{
	return map_for_each(block->customs, func, data);
}

static void
child_setenv(struct block *block, const char *name, const char *value)
{
	if (setenv(name, value, 1) == -1) {
		berrorx(block, "setenv(%s=%s)", name, value);
		_exit(EXIT_ERR_INTERNAL);
	}
}

static void
child_setup_env(struct block *block)
{
	child_setenv(block, "BLOCK_NAME", block_get(block, "name") ? : "");
	child_setenv(block, "BLOCK_INSTANCE", block_get(block, "instance") ? : "");
	child_setenv(block, "BLOCK_INTERVAL", block_get(block, "interval") ? : "");
	child_setenv(block, "BLOCK_BUTTON", block_get(block, "button") ? : "");
	child_setenv(block, "BLOCK_X", block_get(block, "x") ? : "");
	child_setenv(block, "BLOCK_Y", block_get(block, "y") ? : "");
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

	execl(shell, shell, "-c", block->command, (char *) NULL);
	/* Unlikely to reach this point */
	berrorx(block, "exec(%s -c %s)", shell, block->command);
	_exit(EXIT_ERR_INTERNAL);
}

static int block_dump_stderr_line(char *line, size_t num, void *data)
{
	struct block *block = data;

	bdebug(block, "{stderr} %s", line);

	return 0;
}

static void block_dump_stderr(struct block *block)
{
	int err;

	err = io_readlines(block->err, -1, block_dump_stderr_line, block);
	if (err)
		berror(block, "read stderr");
}

static void
mark_as_failed(struct block *block, const char *reason)
{
	const char *instance = block_get(block, "instance") ? : "";
	const char *name = block_get(block, "name") ? : "";
	char short_text[BUFSIZ];
	char full_text[BUFSIZ];

	if (log_level < LOG_WARN)
		return;

	map_clear(block->customs);

	snprintf(full_text, sizeof(full_text), "[%s] %s", name, reason);
	snprintf(short_text, sizeof(short_text), "[%s] ERROR", name);

	block_set(block, "name", name);
	block_set(block, "instance", instance);
	block_set(block, "full_text", full_text);
	block_set(block, "short_text", short_text);
	block_set(block, "color", "#FF0000");
	block_set(block, "urgent", "true");
}

static int block_update_plain_text(char *line, size_t num, void *data)
{
	struct block *block = data;
	static const char * const keys[] = {
		"full_text",
		"short_text",
		"color",
	};
	const char *key;

	if (num >= sizeof(keys) / sizeof(keys[0])) {
		berror(block, "too much lines for plain text update");
		return -EINVAL;
	}

	key = keys[num];

	return block_set(block, key, line);
}

static int block_update_json(char *name, char *value, void *data)
{
	struct block *block = data;

	return block_set(block, name, value);
}

void
block_update(struct block *block)
{
	const char *full_text;
	const char *label;
	size_t count;
	int err;

	/* Reset properties to default before updating from output */
	block_reset(block);

	if (block->interval == INTER_PERSIST)
		count = 1;
	else
		count = -1; /* SIZE_MAX */

	if (block->format == FORMAT_JSON)
		err = json_read(block->out, count, block_update_json, block);
	else
		err = io_readlines(block->out, count, block_update_plain_text,
				   block);

	if (err) {
		berror(block, "failed to read output");
		return mark_as_failed(block, "read failed");
	}

	full_text = block_get(block, "full_text") ? : "";
	label = block_get(block, "label") ? : "";

	if (*full_text && *label) {
		const size_t sz = strlen(full_text) + strlen(label) + 2;
		char concat[sz];
		snprintf(concat, sz, "%s %s", label, full_text);
		block_set(block, "full_text", concat);
	}

	bdebug(block, "updated successfully");
}

int block_click(struct block *block, const struct click *click)
{
	int err;

	err = block_set(block, "button", click->button);
	if (err)
		return err;

	err = block_set(block, "x", click->x);
	if (err)
		return err;

	return block_set(block, "y", click->y);
}

void block_touch(struct block *block)
{
	block->timestamp = time(NULL);
}

void block_spawn(struct block *block)
{
	int out[2], err[2];

	if (!block->command) {
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

	if (block->interval == INTER_PERSIST) {
		if (io_signal(out[0], SIGRTMIN))
			return mark_as_failed(block, "event I/O impossible");
	}

	block->pid = fork();
	if (block->pid == -1) {
		berrorx(block, "fork");
		return mark_as_failed(block, strerror(errno));
	}

	/* Child? */
	if (block->pid == 0) {
		/* Error messages are merged into the parent's stderr... */
		child_setup_env(block);
		child_reset_signals(block);
		child_redirect_write(block, out, STDOUT_FILENO);
		child_redirect_write(block, err, STDERR_FILENO);
		/* ... until here */
		child_exec(block);
	}

	/*
	 * Note: for non-persistent blocks, no need to set the pipe read end as
	 * non-blocking, since it is meant to be read once the child has exited
	 * (and thus the write end is closed and read is available).
	 */

	/* Parent */
	if (close(out[1]) == -1)
		berrorx(block, "close stdout");
	if (close(err[1]) == -1)
		berrorx(block, "close stderr");

	block->out = out[0];
	block->err = err[0];

	bdebug(block, "forked child %d", block->pid);
}

void
block_reap(struct block *block)
{
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

	/* Process successfully reaped, reset the block PID */
	block->pid = 0;

	block_dump_stderr(block);

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

	/* Do not update unless it was meant to terminate */
	if (block->interval == INTER_PERSIST)
		goto close;

	block_update(block);

	/* Exit code takes precedence over the output */
	if (code == EXIT_URGENT)
		block_set(block, "urgent", "true");
close:
	if (close(block->out) == -1)
		berrorx(block, "close stdout");
	if (close(block->err) == -1)
		berrorx(block, "close stderr");

	/* Invalidate descriptors to avoid misdetection after reassignment */
	block->out = block->err = -1;
}

static int block_debug_value(const char *key, const char *value, void *data)
{
	debug("    %s: %s", key, value);

	return 0;
}

static void block_debug(const struct block *block)
{
	block_for_each(block, block_debug_value, NULL);
}

static int block_default(const char *key, const char *value, void *data)
{
	struct block *block = data;

	if (strcmp(key, "command") == 0) {
		if (value && *value != '\0')
			block->command = value;
	} if (strcmp(key, "interval") == 0) {
		if (value && strcmp(value, "once") == 0)
			block->interval = INTER_ONCE;
		else if (value && strcmp(value, "repeat") == 0)
			block->interval = INTER_REPEAT;
		else if (value && strcmp(value, "persist") == 0)
			block->interval = INTER_PERSIST;
		else if (value)
			block->interval = atoi(value);
		else
			block->interval = 0;
	} else if (strcmp(key, "format") == 0) {
		if (value && strcmp(value, "json") == 0)
			block->format = FORMAT_JSON;
		else
			block->format = FORMAT_PLAIN;
	} else if (strcmp(key, "signal") == 0) {
		if (value)
			block->signal = atoi(value);
		else
			block->signal = 0;
	}

	return 0;
}

int block_setup(struct block *block)
{
	int err;

	err = map_for_each(block->defaults, block_default, block);
	if (err)
		return err;

	/* First update (for static blocks and loading labels) */
	block->customs = map_create();
	if (!block->customs)
		return -ENOMEM;

	block_reset(block);

	block_debug(block);

	return 0;
}
