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
#include "json.h"
#include "line.h"
#include "log.h"
#include "sys.h"

const char *block_get(const struct block *block, const char *key)
{
	return map_get(block->env, key);
}

int block_set(struct block *block, const char *key, const char *value)
{
	return map_set(block->env, key, value);
}

static int block_reset(struct block *block)
{
	map_clear(block->env);

	return map_copy(block->env, block->config);
}

int block_for_each(const struct block *block,
		   int (*func)(const char *key, const char *value, void *data),
		   void *data)
{
	return map_for_each(block->env, func, data);
}

static bool block_is_spawned(struct block *block)
{
	return block->pid > 0;
}

static int block_setenv(const char *name, const char *value, void *data)
{
	int err;

	if (!value)
		value = "";

	err = sys_setenv(name, value);
	if (err)
		return err;

	/* Legacy env variables */
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

static int block_update_raw(char *line, size_t num, void *data)
{
	struct block *block = data;
	static const char * const keys[] = {
		"full_text",
		"short_text",
		"color",
		"background",
	};
	const char *key;

	if (num >= sizeof(keys) / sizeof(keys[0])) {
		block_error(block, "too much lines for raw update");
		return -EINVAL;
	}

	key = keys[num];

	return block_set(block, key, line);
}

static int block_update_json(char *name, char *value, void *data)
{
	struct block *block = data;
	char buf[BUFSIZ];
	int err;

	err = json_unescape(value, buf, sizeof(buf));
	if (err)
		return err;

	return block_set(block, name, buf);
}

static int block_stdout(struct block *block)
{
	const char *label, *full_text;
	int out = block->out[0];
	char buf[BUFSIZ];
	size_t count;
	int err;

	if (block->interval == INTERVAL_PERSIST)
		count = 1;
	else
		count = -1; /* SIZE_MAX */

	if (block->format == FORMAT_JSON)
		err = json_read(out, count, block_update_json, block);
	else
		err = line_read(out, count, block_update_raw, block);

	if (err && err != -EAGAIN)
		return err;

	/* Deprecated label */
	label = block_get(block, "label");
	full_text = block_get(block, "full_text");
	if (label && full_text) {
		snprintf(buf, sizeof(buf), "%s%s", label, full_text);
		err = block_set(block, "full_text", buf);
		if (err)
			return err;
	}

	return 0;
}

int block_update(struct block *block)
{
	int err;

	/* Reset properties to default before updating from output */
	err = block_reset(block);
	if (err)
		return err;

	err = block_stdout(block);
	if (err)
		return err;

	/* Exit code takes precedence over the output */
	if (block->code == EXIT_URGENT) {
		err = block_set(block, "urgent", "true");
		if (err)
			return err;
	}

	block_debug(block, "updated successfully");

	return 0;
}

static int block_send_key(const char *key, const char *value, void *data)
{
	struct block *block = data;
	char buf[BUFSIZ];
	int err;

	if (!json_is_valid(value)) {
		err = json_escape(value, buf, sizeof(buf));
		if (err)
			return err;

		value = buf;
	}

	dprintf(block->in[1], ",\"%s\":%s", key, value);

	return 0;
}

static int block_send_json(struct block *block)
{
	dprintf(block->in[1], "{\"\":\"\"");
	block_for_each(block, block_send_key, block);
	dprintf(block->in[1], "}\n");
}

/* Push data to forked process through the open stdin pipe */
static int block_send(struct block *block)
{
	const char *button = block_get(block, "button");

	if (!button) {
		block_error(block, "no click data to send");
		return -EINVAL;
	}

	if (!block_is_spawned(block)) {
		block_debug(block, "process not spawned");
		return 0;
	}

	if (block->format == FORMAT_JSON)
		block_send_json(block);
	else
		dprintf(block->in[1], "%s\n", button);

	return 0;
}

int block_click(struct block *block)
{
	block_debug(block, "clicked");

	if (block->interval == INTERVAL_PERSIST)
		return block_send(block);

	return block_spawn(block);
}

void block_touch(struct block *block)
{
	unsigned long now;
	int err;

	err = sys_gettime(&now);
	if (err) {
		block_error(block, "failed to touch block");
		return;
	}

	if (block->timestamp == now) {
		block_debug(block, "looping too fast");
		return;
	}

	block->timestamp = now;
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

static int block_child_stdin(struct block *block)
{
	int err;

	if (block->interval == INTERVAL_PERSIST) {
		err = sys_close(block->in[1]);
		if (err)
			return err;
	} else {
		err = sys_open("/dev/null", &block->in[0]);
		if (err)
			return err;
	}

	err = sys_dup(block->in[0], STDIN_FILENO);
	if (err)
		return err;

	return sys_close(block->in[0]);
}

static int block_child_stdout(struct block *block)
{
	int err;

	err = sys_close(block->out[0]);
	if (err)
		return err;

	err = sys_dup(block->out[1], STDOUT_FILENO);
	if (err)
		return err;

	return sys_close(block->out[1]);
}

static int block_child_stderr(struct block *block)
{
	int err;

	err = sys_close(block->err[0]);
	if (err)
		return err;

	err = sys_dup(block->err[1], STDERR_FILENO);
	if (err)
		return err;

	return sys_close(block->err[1]);
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

	err = block_child_stdin(block);
	if (err)
		return err;

	err = block_child_stdout(block);
	if (err)
		return err;

	err = block_child_stderr(block);
	if (err)
		return err;

	/* ... until here */
	return block_child_exec(block);
}

static int block_parent_stdin(struct block *block)
{
	/* Close read end of stdin pipe */
	if (block->interval == INTERVAL_PERSIST)
		return sys_close(block->in[0]);

	return 0;
}

static int block_parent_stdout(struct block *block)
{
	int err;

	/* Close write end of stdout pipe */
	err = sys_close(block->out[1]);
	if (err)
		return err;

	if (block->interval == INTERVAL_PERSIST)
		return sys_async(block->out[0], SIGRTMIN);

	return 0;
}

static int block_parent_stderr(struct block *block)
{
	/* Close write end of stderr pipe */
	return sys_close(block->err[1]);
}

static int block_parent(struct block *block)
{
	int err;

	err = block_parent_stdin(block);
	if (err)
		return err;

	err = block_parent_stdout(block);
	if (err)
		return err;

	err = block_parent_stderr(block);
	if (err)
		return err;

	block_debug(block, "forked child %d", block->pid);

	return 0;
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

	return block_parent(block);
}

static int block_open(struct block *block)
{
	int err;

	err = sys_pipe(block->err);
	if (err)
		return err;

	err = sys_pipe(block->out);
	if (err)
		return err;

	if (block->interval == INTERVAL_PERSIST)
		return sys_pipe(block->in);

	return 0;
}

int block_spawn(struct block *block)
{
	int err;

	if (!block->command) {
		block_debug(block, "no command, skipping");
		return 0;
	}

	if (block_is_spawned(block)) {
		block_debug(block, "process already spawned");
		return 0;
	}

	err = block_open(block);
	if (err)
		return err;

	return block_fork(block);
}

static int block_wait(struct block *block)
{
	int err;

	if (block->pid <= 0) {
		block_debug(block, "not spawned yet");
		return -EAGAIN;
	}

	err = sys_waitpid(block->pid, &block->code);
	if (err)
		return err;

	block_debug(block, "process %d exited with %d", block->pid, block->code);

	/* Process successfully reaped, reset the block PID */
	block->pid = 0;

	if (block->code == EXIT_ERR_INTERNAL)
		return -ECHILD;

	return 0;
}

void block_close(struct block *block)
{
	int err;

	/* Invalidate descriptors to avoid misdetection after reassignment */
	if (block->interval == INTERVAL_PERSIST) {
		err = sys_close(block->in[1]);
		if (err)
			block_error(block, "failed to close stdin");

		block->in[1] = -1;
	}

	err = sys_close(block->out[0]);
	if (err)
		block_error(block, "failed to close stdout");

	block->out[0] = -1;

	err = sys_close(block->err[0]);
	if (err)
		block_error(block, "failed to close stderr");

	block->err[0] = -1;
}

static int block_stderr_line(char *line, size_t num, void *data)
{
	struct block *block = data;

	block_debug(block, "&stderr:%02d: %s", num, line);

	return 0;
}

static int block_stderr(struct block *block)
{
	int err;

	err = line_read(block->err[0], -1, block_stderr_line, block);
	if (err && err != -EAGAIN)
		return err;

	return 0;
}

int block_reap(struct block *block)
{
	int err;

	err = block_wait(block);
	if (err) {
		if (err == -EAGAIN)
			return 0;

		block_error(block, "Internal error");
		return err;
	}

	err = block_stderr(block);
	if (err)
		return err;

	switch (block->code) {
	case 0:
	case EXIT_URGENT:
		break;
	case 126:
		block_error(block, "Command '%s' not executable",
			    block->command);
		break;
	case 127:
		block_error(block, "Command '%s' not found or missing dependency",
			    block->command);
		break;
	default:
		block_error(block, "Command '%s' exited unexpectedly with code %d",
			    block->command, block->code);
		break;
	}

	return 0;
}

int block_setup(struct block *block)
{
	const struct map *config = block->config;
	const char *value;
	int err;

	value = map_get(config, "command");
	if (value && *value != '\0')
		block->command = value;

	value = map_get(config, "interval");
	if (!value)
		block->interval = 0;
	else if (strcmp(value, "once") == 0)
		block->interval = INTERVAL_ONCE;
	else if (strcmp(value, "repeat") == 0)
		block->interval = INTERVAL_REPEAT;
	else if (strcmp(value, "persist") == 0)
		block->interval = INTERVAL_PERSIST;
	else
		block->interval = atoi(value);

	value = map_get(config, "format");
	if (value && strcmp(value, "json") == 0)
		block->format = FORMAT_JSON;
	else
		block->format = FORMAT_RAW;

	value = map_get(config, "signal");
	if (!value)
		block->signal = 0;
	else
		block->signal = atoi(value);

	block->env = map_create();
	if (!block->env)
		return -ENOMEM;

	err = block_reset(block);
	if (err)
		return err;

	block_debug(block, "new block");

	return 0;
}
