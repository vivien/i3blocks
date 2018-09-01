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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "click.h"
#include "json.h"
#include "line.h"
#include "log.h"
#include "sys.h"

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

static int block_setenv(const char *name, const char *value, void *data)
{
	if (!value)
		value = "";

	if (strcmp(name, "name") == 0)
		return sys_setenv("BLOCK_NAME", value);
	if (strcmp(name, "instance") == 0)
		return sys_setenv("BLOCK_INSTANCE", value);
	if (strcmp(name, "interval") == 0)
		return sys_setenv("BLOCK_INTERVAL", value);
	if (strcmp(name, "button") == 0)
		return sys_setenv("BLOCK_BUTTON", value);
	if (strcmp(name, "x") == 0)
		return sys_setenv("BLOCK_X", value);
	if (strcmp(name, "y") == 0)
		return sys_setenv("BLOCK_Y", value);

	return 0;
}

static int block_child_env(struct block *block)
{
	return block_for_each(block, block_setenv, NULL);
}

static void
mark_as_failed(struct block *block, const char *reason)
{
	char short_text[BUFSIZ];
	char full_text[BUFSIZ];
	const char *name;

	if (log_level < LOG_WARN)
		return;

	block_reset(block);

	name = block_get(block, "name") ? : "";
	snprintf(full_text, sizeof(full_text), "[%s] %s", name, reason);
	snprintf(short_text, sizeof(short_text), "[%s] ERROR", name);

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

static int block_stdout(struct block *block)
{
	int out = block->out[0];
	size_t count;

	if (block->interval == INTER_PERSIST)
		count = 1;
	else
		count = -1; /* SIZE_MAX */

	if (block->format == FORMAT_JSON)
		return json_read(out, count, block_update_json, block);
	else
		return line_read(out, count, block_update_plain_text, block);
}

int block_update(struct block *block)
{
	const char *full_text;
	const char *label;
	int err;

	/* Reset properties to default before updating from output */
	err = block_reset(block);
	if (err)
		return err;

	err = block_stdout(block);
	if (err)
		return err;

	full_text = block_get(block, "full_text") ? : "";
	label = block_get(block, "label") ? : "";

	if (*full_text && *label) {
		const size_t sz = strlen(full_text) + strlen(label) + 2;
		char concat[sz];
		snprintf(concat, sz, "%s %s", label, full_text);
		err = block_set(block, "full_text", concat);
		if (err)
			return err;
	}

	/* Exit code takes precedence over the output */
	if (block->code == EXIT_URGENT) {
		err = block_set(block, "urgent", "true");
		if (err)
			return err;
	}

	bdebug(block, "updated successfully");

	return 0;
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
	block->timestamp = sys_time();
}

static int block_child_sig(struct block *block)
{
	sigset_t set;
	int err;

	/* It'd be safe to assume that all signals are unblocked by default */
	err = sys_sigfillset(&set);
	if (err)
		return err;

	return sys_sigunblock(&set);
}

static int block_child_pipe(int *pipe, int fd)
{
	int err;

	/* Close read end of the pipe */
	err = sys_close(pipe[0]);
	if (err)
		return err;

	/* Rebound write end of the pipe to fd */
	err = sys_dup(pipe[1], fd);
	if (err)
		return err;

	/* Close the superfluous descriptor */
	return sys_close(pipe[1]);
}

static int block_child_exec(struct block *block)
{
	return sys_execsh(block->command);
}

static int block_child(struct block *block)
{
	int err;

	/* Error messages are merged into the parent's stderr... */
	err = block_child_env(block);
	if (err)
		return err;

	err = block_child_sig(block);
	if (err)
		return err;

	err = block_child_pipe(block->out, STDOUT_FILENO);
	if (err)
		return err;

	err = block_child_pipe(block->err, STDERR_FILENO);
	if (err)
		return err;

	/* ... until here */
	return block_child_exec(block);
}

static int block_parent(struct block *block)
{
	int err;

	/*
	 * Note: for non-persistent blocks, no need to set the pipe read end as
	 * non-blocking, since it is meant to be read once the child has exited
	 * (and thus the write end is closed and read is available).
	 */

	/* Close write end of stdout pipe */
	err = sys_close(block->out[1]);
	if (err)
		return err;

	/* Close write end of stderr pipe */
	return sys_close(block->err[1]);
}

static int block_fork(struct block *block)
{
	int err;

	err = sys_fork(&block->pid);
	if (err)
		return err;

	if (block->pid == 0) {
		err = block_child(block);
		if (err)
			sys_exit(EXIT_ERR_INTERNAL);
	}

	bdebug(block, "forked child %d", block->pid);

	return block_parent(block);
}

int block_spawn(struct block *block)
{
	int err;

	if (!block->command) {
		bdebug(block, "no command, skipping");
		return 0;
	}

	if (block->pid > 0) {
		bdebug(block, "process already spawned");
		return 0;
	}

	err = sys_pipe(block->out);
	if (err)
		return err;

	err = sys_pipe(block->err);
	if (err)
		return err;

	if (block->interval == INTER_PERSIST) {
		err = sys_async(block->out[0], SIGRTMIN);
		if (err)
			return err;
	}

	return block_fork(block);
}

static int block_wait(struct block *block)
{
	int err;

	if (block->pid <= 0) {
		bdebug(block, "not spawned yet");
		return -EAGAIN;
	}

	err = sys_waitpid(block->pid, &block->code);
	if (err)
		return err;

	bdebug(block, "process %d exited with %d", block->pid, block->code);

	/* Process successfully reaped, reset the block PID */
	block->pid = 0;

	if (block->code == EXIT_ERR_INTERNAL)
		return -ECHILD;

	return 0;
}

static void block_close(struct block *block)
{
	int err;

	err = sys_close(block->out[0]);
	if (err)
		bdebug(block, "failed to close stdout");

	block->out[0] = -1;

	err = sys_close(block->err[0]);
	if (err)
		bdebug(block, "failed to close stderr");

	block->err[0] = -1;
}

static int block_stderr_line(char *line, size_t num, void *data)
{
	struct block *block = data;

	bdebug(block, "{stderr} %s", line);

	return 0;
}

static int block_stderr(struct block *block)
{
	return line_read(block->err[0], -1, block_stderr_line, block);
}

int block_reap(struct block *block)
{
	int err;

	err = block_wait(block);
	if (err) {
		if (err == -EAGAIN)
			return 0;

		mark_as_failed(block, "internal error");
		return err;
	}

	err = block_stderr(block);
	if (err)
		goto close;

	if (block->code != 0 && block->code != EXIT_URGENT) {
		char reason[32];

		sprintf(reason, "bad exit code %d", block->code);
		mark_as_failed(block, reason);
		goto close;
	}

	/* Do not update unless it was meant to terminate */
	if (block->interval == INTER_PERSIST)
		goto close;

	err = block_update(block);
	if (err)
		mark_as_failed(block, "failed to update");
close:
	/* Invalidate descriptors to avoid misdetection after reassignment */
	block_close(block);

	return err;
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

	bdebug(block, "new block");

	return 0;
}
