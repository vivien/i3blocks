/*
 * bar.c - status line handling functions
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

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bar.h"
#include "block.h"
#include "config.h"
#include "json.h"
#include "line.h"
#include "log.h"
#include "map.h"
#include "sched.h"
#include "sys.h"
#include "term.h"

static void bar_read(struct bar *bar)
{
	int err;

	err = i3bar_click(bar);
	if (err)
		bar_error(bar, "failed to read bar");
}

static void bar_print(struct bar *bar)
{
	int err;

	err = i3bar_print(bar);
	if (err)
		fatal("failed to print bar!");
}

static int bar_start(struct bar *bar)
{
	int err;

	err = i3bar_start(bar);
	if (err)
		return err;

	debug("bar started");

	return 0;
}

static void bar_stop(struct bar *bar)
{
	i3bar_stop(bar);

	debug("bar stopped");
}

static void bar_poll_timed(struct bar *bar)
{
	struct block *block = bar->blocks;

	while (block) {
		/* spawn unless it is only meant for click or signal */
		if (block->interval != 0) {
			block_spawn(block);
			block_touch(block);
		}

		block = block->next;
	}
}

static void bar_poll_expired(struct bar *bar)
{
	struct block *block = bar->blocks;

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

static void bar_poll_signaled(struct bar *bar, int sig)
{
	struct block *block = bar->blocks;

	while (block) {
		if (block->signal == sig) {
			block_debug(block, "signaled");
			block_spawn(block);
			block_touch(block);
		}

		block = block->next;
	}
}

static void bar_poll_exited(struct bar *bar)
{
	struct block *block;
	pid_t pid;
	int err;

	for (;;) {
		err = sys_waitid(&pid);
		if (err)
			break;

		/* Find the dead process */
		block = bar->blocks;
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
				block_update(block);
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
}

static void bar_poll_readable(struct bar *bar, const int fd)
{
	struct block *block = bar->blocks;

	while (block) {
		if (block->out[0] == fd) {
			block_debug(block, "readable");
			block_update(block);
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

static int bar_setup(struct bar *bar)
{
	struct block *block = bar->blocks;
	sigset_t *set = &bar->sigset;
	unsigned long sleeptime = 0;
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

	err = sys_sigemptyset(set);
	if (err)
		return err;

	/* Control signals */
	err = sys_sigaddset(set, SIGTERM);
	if (err)
		return err;

	err = sys_sigaddset(set, SIGINT);
	if (err)
		return err;

	/* Timer signal */
	err = sys_sigaddset(set, SIGALRM);
	if (err)
		return err;

	/* Block updates (forks) */
	err = sys_sigaddset(set, SIGCHLD);
	if (err)
		return err;

	/* Deprecated signals */
	err = sys_sigaddset(set, SIGUSR1);
	if (err)
		return err;

	err = sys_sigaddset(set, SIGUSR2);
	if (err)
		return err;

	/* Click signal */
	err = sys_sigaddset(set, SIGIO);
	if (err)
		return err;

	/* I/O Possible signal for persistent blocks */
	err = sys_sigaddset(set, SIGRTMIN);
	if (err)
		return err;

	/* Real-time signals for blocks */
	for (sig = SIGRTMIN + 1; sig <= SIGRTMAX; sig++) {
		err = sys_sigaddset(set, sig);
		if (err)
			return err;
	}

	/* Block signals for which we are interested in waiting */
	err = sys_sigsetmask(set);
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

static void bar_teardown(struct bar *bar)
{
	struct block *block = bar->blocks;
	int err;

	/* Disable event I/O for blocks (persistent) */
	while (block) {
		if (block->interval == INTERVAL_PERSIST) {
			err = sys_async(block->out[0], 0);
			if (err)
				block_error(block, "failed to disable event I/O");
		}

		block = block->next;
	}

	/* Disable event I/O for stdin (clicks) */
	err = sys_async(STDIN_FILENO, 0);
	if (err)
		error("failed to disable event I/O on stdin");

	/*
	 * Unblock signals (so subsequent syscall can be interrupted)
	 * and wait for child processes termination.
	 */
	err = sys_sigunblock(&bar->sigset);
	if (err)
		error("failed to unblock signals");

	err = sys_waitanychild();
	if (err)
		error("failed to wait for any child");

	debug("bar tear down");
}

static int bar_poll(struct bar *bar)
{
	int sig, fd;
	int err;

	err = bar_setup(bar);
	if (err)
		return err;

	/* Initial display (for static blocks and loading labels) */
	bar_print(bar);

	/* First forks (for commands with an interval) */
	bar_poll_timed(bar);

	while (1) {
		err = sys_sigwaitinfo(&bar->sigset, &sig, &fd);
		if (err) {
			/* Hiding the bar may interrupt this system call */
			if (err == -EINTR)
				continue;
			break;
		}

		if (sig == SIGTERM || sig == SIGINT)
			break;

		if (sig == SIGALRM) {
			bar_poll_expired(bar);
			continue;
		}

		if (sig == SIGCHLD) {
			bar_poll_exited(bar);
			bar_print(bar);
			continue;
		}

		if (sig == SIGIO) {
			bar_read(bar);
			continue;
		}

		if (sig == SIGRTMIN) {
			bar_poll_readable(bar, fd);
			bar_print(bar);
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

static void bar_destroy(struct bar *bar)
{
	struct block *block = bar->blocks;
	struct block *next;

	bar_stop(bar);

	while (block) {
		next = block->next;
		block_destroy(block);
		block = next;
	}

	free(bar);
}

static struct bar *bar_create(bool term)
{
	struct bar *bar;
	int err;

	bar = calloc(1, sizeof(struct bar));
	if (!bar)
		return NULL;

	bar->blocks = block_create(bar, NULL);
	if (!bar->blocks) {
		bar_destroy(bar);
		return NULL;
	}

	bar->term = term;

	err = bar_start(bar);
	if (err) {
		bar_destroy(bar);
		return NULL;
	}

	return bar;
}

static int bar_config_cb(struct map *map, void *data)
{
	struct bar *bar = data;
	struct block *block = bar->blocks;

	while (block->next)
		block = block->next;

	block->next = block_create(bar, map);

	map_destroy(map);

	if (!block->next)
		return -ENOMEM;

	return 0;
}

static void bar_load(struct bar *bar, const char *path)
{
	int err;

	err = config_load(path, bar_config_cb, bar);
	if (err)
		bar_fatal(bar, "Failed to load configuration file %s", path);
}

int bar_init(bool term, const char *path)
{
	struct bar *bar;
	int err;

	bar = bar_create(term);
	if (!bar)
		return -ENOMEM;

	bar_load(bar, path);

	err = bar_poll(bar);

	bar_destroy(bar);

	return err;
}
