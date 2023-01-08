/*
 * block.c - update of a single status line block
 * Copyright (C) 2014-2019  Vivien Didelot
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "config.h"
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

int block_reset(struct block *block)
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

	if (strcmp(name, "workdir") == 0)
		return sys_chdir(value);

	return 0;
}

static int block_read(const struct block *block, size_t count, struct map *map)
{
	if (block->format == FORMAT_JSON)
		return json_read(block->out, count, map);
	else
		return i3bar_read(block->out, count, map);
}

static int block_update(struct block *block, const struct map *map)
{
	const char *label, *full_text;
	char buf[BUFSIZ];
	int err;

	err = block_reset(block);
	if (err)
		return err;

	err = map_copy(block->env, map);
	if (err)
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

	/* Exit code takes precedence over the output */
	if (block->code == EXIT_URGENT) {
		err = block_set(block, "urgent", "true");
		if (err)
			return err;
	}

	return 0;
}

int block_drain(struct block *block)
{
	struct map *map;
	int err;

	map = map_create();
	if (!map)
		return -ENOMEM;

	if (block->interval == INTERVAL_PERSIST) {
		for (;;) {
			err = block_read(block, 1, map);
			if (err) {
				if (err == -EAGAIN)
					err = 0;
				break;
			}

			err = block_update(block, map);
			if (err)
				block_error(block, "failed to update");

			map_clear(map);
		}
	} else {
		err = block_read(block, -1, map);
		if (err == 0)
			err = -EINVAL; /* Unlikely more than SIZE_MAX lines */

		if (err == -EAGAIN) {
			err = block_update(block, map);
			if (err)
				block_error(block, "failed to update");
		}
	}

	map_destroy(map);

	return err;
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

	dprintf(block->in, ",\"%s\":%s", key, value);

	return 0;
}

static int block_send_json(struct block *block)
{
	int err;

	dprintf(block->in, "{\"\":\"\"");
	err = block_for_each(block, block_send_key, block);
	dprintf(block->in, "}\n");

	return err;
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
		block_error(block, "persistent block not spawned");
		return 0;
	}

	if (block->format == FORMAT_JSON)
		return block_send_json(block);

	dprintf(block->in, "%s\n", button);

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

static int block_child(struct block *block)
{
	int err;

	err = block_for_each(block, block_setenv, NULL);
	if (err)
		return err;

	err = sys_sigsetmask(&block->bar->sigset, NULL);
	if (err)
		return err;

	err = sys_dup(block->in, STDIN_FILENO);
	if (err)
		return err;

	err = sys_close(block->in);
	if (err)
		return err;

	err = sys_dup(block->out, STDOUT_FILENO);
	if (err)
		return err;

	err = sys_close(block->out);
	if (err)
		return err;

	return sys_execsh(block->command);
}

static int block_fork(struct block *block)
{
	int out[2];
	int in[2];
	int err;

	err = sys_pipe(out);
	if (err)
		return err;

	err = sys_cloexec(out[0]);
	if (err)
		return err;

	if (block->interval == INTERVAL_PERSIST) {
		err = sys_pipe(in);
		if (err)
			return err;

		err = sys_cloexec(in[1]);
		if (err)
			return err;
	} else {
		err = sys_open("/dev/null", &in[0]);
		if (err)
			return err;
	}

	err = sys_fork(&block->pid);
	if (err)
		return err;

	if (block->pid == 0) {
		block->in = in[0];
		block->out = out[1];

		err = block_child(block);
		if (err)
			sys_exit(EXIT_ERR_INTERNAL);
	}

	err = sys_close(in[0]);
	if (err)
		return err;

	err = sys_close(out[1]);
	if (err)
		return err;

	block->in = in[1];
	block->out = out[0];

	if (block->interval == INTERVAL_PERSIST) {
		err = sys_async(block->out, SIGRTMIN);
		if (err)
			return err;
	}

	block_debug(block, "forked child %d", block->pid);

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
		err = sys_close(block->in);
		if (err)
			block_error(block, "failed to close stdin");

		block->in = -1;
	}

	err = sys_close(block->out);
	if (err)
		block_error(block, "failed to close stdout");

	block->out = -1;
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

static int i3blocks_setup(struct block *block)
{
	const char *value;

	value = map_get(block->config, "command");
	if (value && *value != '\0')
		block->command = value;

	value = map_get(block->config, "interval");
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

	value = map_get(block->config, "format");
	if (value && strcmp(value, "json") == 0)
		block->format = FORMAT_JSON;
	else
		block->format = FORMAT_RAW;

	value = map_get(block->config, "signal");
	if (!value)
		block->signal = 0;
	else
		block->signal = atoi(value);

	return 0;
}

int block_setup(struct block *block)
{
	int err;

	err = i3bar_setup(block);
	if (err)
		return err;

	err = i3blocks_setup(block);
	if (err)
		return err;

	err = block_reset(block);
	if (err)
		return err;

	block_debug(block, "new block");

	return 0;
}

void block_destroy(struct block *block)
{
	if (block->config)
		map_destroy(block->config);
	if (block->env)
		map_destroy(block->env);
	if (block->name)
		free(block->name);
	free(block);
}

struct block *block_create(struct block *bar, const struct map *config)
{
	struct block *block;
	int err;

	block = calloc(1, sizeof(struct block));
	if (!block)
		return NULL;

	block->bar = bar;

	block->config = map_create();
	if (!block->config) {
		block_destroy(block);
		return NULL;
	}

	if (config) {
		err = map_copy(block->config, config);
		if (err) {
			block_destroy(block);
			return NULL;
		}
	}

	block->env = map_create();
	if (!block->env) {
		block_destroy(block);
		return NULL;
	}

	return block;
}

void block_printf(struct block *block, int lvl, const char *fmt, ...)
{
	char buf[BUFSIZ];
	va_list ap;
	int err;

	if (lvl > log_level)
		return;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	err = i3bar_printf(block, lvl, buf);
	if (err)
		fatal("failed to format message for block %s: %s", block->name, buf);
}

static void bar_read(struct block *bar)
{
	int err;

	err = i3bar_click(bar);
	if (err)
		block_error(bar, "failed to read bar");
}

static void bar_print(struct block *bar)
{
	int err;

	err = i3bar_print(bar);
	if (err)
		fatal("failed to print bar!");
}

static int bar_start(struct block *bar)
{
	int err;

	err = i3bar_start(bar);
	if (err)
		return err;

	debug("bar started");

	return 0;
}

static void bar_stop(struct block *bar)
{
	i3bar_stop(bar);

	debug("bar stopped");
}

static void bar_poll_timed(struct block *bar)
{
	struct block *block = bar->next;

	while (block) {
		/* spawn unless it is only meant for click or signal */
		if (block->interval != 0) {
			block_spawn(block);
			block_touch(block);
		}

		block = block->next;
	}
}

static void bar_poll_expired(struct block *bar)
{
	struct block *block = bar->next;

	while (block) {
		if (block->interval > 0) {
			const unsigned long next_update = block->timestamp + block->interval;
			unsigned long now;
			int err;

			err = sys_gettime(&now);
			if (err)
				return;

			if (((long) (next_update - now)) <= 0) {
				block_debug(block, "expired");
				block_spawn(block);
				block_touch(block);
			}
		}

		block = block->next;
	}
}

static void bar_poll_signaled(struct block *bar, int sig)
{
	struct block *block = bar->next;

	while (block) {
		if (block->signal == sig) {
			block_debug(block, "signaled");
			block_spawn(block);
			block_touch(block);
		}

		block = block->next;
	}
}

static void bar_poll_exited(struct block *bar)
{
	struct block *block;
	pid_t pid;
	int err;

	for (;;) {
		err = sys_waitid(&pid);
		if (err)
			break;

		/* Find the dead process */
		block = bar->next;
		while (block) {
			if (block->pid == pid)
				break;

			block = block->next;
		}

		if (block) {
			block_debug(block, "exited");
			block_reap(block);
			if (block->interval == INTERVAL_PERSIST) {
				block_debug(block, "unexpected exit?");
			} else {
				block_drain(block);
			}
			block_close(block);
			if (block->interval == INTERVAL_REPEAT) {
				block_spawn(block);
				block_touch(block);
			}
		} else {
			error("unknown child process %d", pid);
			err = sys_waitpid(pid, NULL);
			if (err)
				break;
		}
	}

	bar_print(bar);
}

static void bar_poll_flushed(struct block *bar, int sig, int fd)
{
	struct block *block = bar->next;

	if (sig == SIGIO) {
		bar_read(bar);
		return;
	};

	while (block) {
		if (block->out == fd) {
			block_debug(block, "flushed");
			block_drain(block);
			bar_print(bar);
			break;
		}

		block = block->next;
	}
}

static int gcd(int a, int b)
{
	while (b != 0)
		a %= b, a ^= b, b ^= a, a ^= b;

	return a;
}

static int bar_setup(struct block *bar)
{
	struct block *block = bar->next;
	unsigned long sleeptime = 0;
	sigset_t sigset;
	int sig;
	int err;

	while (block) {
		err = block_setup(block);
		if (err)
			return err;

		/* The maximum sleep time is actually the GCD
		 * between all positive block intervals.
		 */
		if (block->interval > 0) {
			if (sleeptime > 0)
				sleeptime = gcd(sleeptime, block->interval);
			else
				sleeptime = block->interval;
		}

		block = block->next;
	}

	err = sys_sigfillset(&sigset);
	if (err)
		return err;

	err = sys_sigsetmask(&sigset, &bar->sigset);
	if (err)
		return err;

	if (sleeptime) {
		err = sys_setitimer(sleeptime);
		if (err)
			return err;
	}

	err = sys_cloexec(STDIN_FILENO);
	if (err)
		return err;

	/* Setup event I/O for stdin (clicks) */
	err = sys_async(STDIN_FILENO, SIGIO);
	if (err)
		return err;

	debug("bar set up");

	return 0;
}

static void bar_teardown(struct block *bar)
{
	struct block *block = bar->next;
	int err;

	/* Disable event I/O for blocks (persistent) */
	while (block) {
		if (block->interval == INTERVAL_PERSIST) {
			err = sys_async(block->out, 0);
			if (err)
				block_error(block, "failed to disable event I/O");
		}

		block = block->next;
	}

	/* Disable event I/O for stdin (clicks) */
	err = sys_async(STDIN_FILENO, 0);
	if (err)
		error("failed to disable event I/O on stdin");

	/* Restore original sigset (so subsequent syscall can be interrupted) */
	err = sys_sigsetmask(&bar->sigset, NULL);
	if (err)
		error("failed to set signal mask");

	/* Wait for child processes termination */
	err = sys_waitanychild();
	if (err)
		error("failed to wait for any child");

	debug("bar tear down");
}

static int bar_poll(struct block *bar)
{
	sigset_t sigset;
	int sig, fd;
	int err;

	err = bar_setup(bar);
	if (err)
		return err;

	/* Initial display (for static blocks and loading labels) */
	bar_print(bar);

	/* First forks (for commands with an interval) */
	bar_poll_timed(bar);

	err = sys_sigfillset(&sigset);
	if (err)
		return err;

	while (1) {
		err = sys_sigwaitinfo(&sigset, &sig, &fd);
		if (err) {
			/* Hiding the bar may interrupt this system call */
			if (err == -EINTR)
				continue;
			break;
		}

		debug("received signal %d (%s) fd %d", sig, strsignal(sig), fd);

		if (sig == SIGTERM || sig == SIGINT)
			break;

		if (sig == SIGALRM) {
			bar_poll_expired(bar);
			continue;
		}

		if (sig == SIGCHLD) {
			bar_poll_exited(bar);
			continue;
		}

		if (sig == SIGIO || sig == SIGRTMIN) {
			bar_poll_flushed(bar, sig, fd);
			continue;
		}

		if (sig > SIGRTMIN && sig <= SIGRTMAX) {
			bar_poll_signaled(bar, sig - SIGRTMIN);
			continue;
		}

		if (sig == SIGUSR1 || sig == SIGUSR2) {
			error("SIGUSR{1,2} are deprecated, ignoring.");
			continue;
		}

		debug("unhandled signal %d", sig);
	}

	bar_teardown(bar);

	return err;
}

static void bar_destroy(struct block *bar)
{
	struct block *block = bar->next;
	struct block *next;

	bar_stop(bar);

	while (block) {
		next = block->next;
		block_destroy(block);
		block = next;
	}

	free(bar);
}

static struct block *bar_create(bool term)
{
	struct block *bar;
	int err;

	bar = calloc(1, sizeof(struct block));
	if (!bar)
		return NULL;

	bar->term = term;

	err = bar_start(bar);
	if (err) {
		bar_destroy(bar);
		return NULL;
	}

	return bar;
}

static int bar_config_cb(const struct map *map, void *data)
{
	struct block *bar = data;
	struct block *block;
	struct block *prev;

	block = block_create(bar, map);
	if (!block)
		return -ENOMEM;

	if (bar->next) {
		prev = bar->next;
		while (prev->next)
			prev = prev->next;
		prev->next = block;
	} else {
		bar->next = block;
	}

	return 0;
}

static void bar_load(struct block *bar, const char *path)
{
	int err;

	err = config_load(path, bar_config_cb, bar);
	if (err)
		block_fatal(bar, "Failed to load configuration file %s", path);
}

int bar_init(bool term, const char *path)
{
	struct block *bar;
	int err;

	bar = bar_create(term);
	if (!bar)
		return -ENOMEM;

	bar_load(bar, path);

	err = bar_poll(bar);

	bar_destroy(bar);

	return err;
}
