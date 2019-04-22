/*
 * sched.c - scheduling of block updates (timeout, signal or click)
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
#include <string.h>
#include <unistd.h>

#include "bar.h"
#include "block.h"
#include "log.h"
#include "sys.h"

static sigset_t set;

static int
gcd(int a, int b)
{
	while (b != 0)
		a %= b, a ^= b, b ^= a, a ^= b;

	return a;
}

static unsigned int
longest_sleep(struct bar *bar)
{
	struct block *block = bar->blocks;
	unsigned int time = 0;

	if (block->interval > 0)
		time = block->interval;

	/* The maximum sleep time is actually the GCD between all block intervals */
	while (block->next) {
		block = block->next;
		if (block->interval > 0)
			time = gcd(time, block->interval);
	}

	return time;
}

static int
setup_timer(struct bar *bar)
{
	const unsigned sleeptime = longest_sleep(bar);
	int err;

	if (!sleeptime) {
		debug("no timer needed");
		return 0;
	}

	err = sys_setitimer(sleeptime);
	if (err)
		return err;

	debug("starting timer with interval of %d seconds", sleeptime);
	return 0;
}

static int
setup_signals(void)
{
	int sig;
	int err;

	err = sys_sigemptyset(&set);
	if (err)
		return err;

	/* Control signals */
	err = sys_sigaddset(&set, SIGTERM);
	if (err)
		return err;

	err = sys_sigaddset(&set, SIGINT);
	if (err)
		return err;

	/* Timer signal */
	err = sys_sigaddset(&set, SIGALRM);
	if (err)
		return err;

	/* Block updates (forks) */
	err = sys_sigaddset(&set, SIGCHLD);
	if (err)
		return err;

	/* Deprecated signals */
	err = sys_sigaddset(&set, SIGUSR1);
	if (err)
		return err;

	err = sys_sigaddset(&set, SIGUSR2);
	if (err)
		return err;

	/* Click signal */
	err = sys_sigaddset(&set, SIGIO);
	if (err)
		return err;

	/* I/O Possible signal for persistent blocks */
	err = sys_sigaddset(&set, SIGRTMIN);
	if (err)
		return err;

	/* Real-time signals for blocks */
	for (sig = SIGRTMIN + 1; sig <= SIGRTMAX; ++sig) {
		debug("provide signal %d (%s)", sig, strsignal(sig));
		err = sys_sigaddset(&set, sig);
		if (err)
			return err;
	}

	/* Block signals for which we are interested in waiting */
	return sys_sigsetmask(&set);
}

int
sched_init(struct bar *bar)
{
	int flags;
	int err;

	err = setup_signals();
	if (err)
		return err;

	err = setup_timer(bar);
	if (err)
		return err;

	err = sys_cloexec(STDIN_FILENO);
	if (err)
		return err;

	/* Setup event I/O for stdin (clicks) */
	return sys_async(STDIN_FILENO, SIGIO);
}

static void sched_poll_timed(struct bar *bar)
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

static void sched_poll_expired(struct bar *bar)
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

static void sched_poll_signaled(struct bar *bar, int sig)
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

static void sched_poll_exited(struct bar *bar)
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

static void sched_poll_readable(struct bar *bar, const int fd)
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

void
sched_start(struct bar *bar)
{
	struct block *block = bar->blocks;
	int sig, fd;
	int err;

	/* First forks (for commands with an interval) */
	sched_poll_timed(bar);

	while (1) {
		err = sys_sigwaitinfo(&set, &sig, &fd);
		if (err) {
			/* Hiding the bar may interrupt this system call */
			if (err == -EINTR)
				continue;
			break;
		}

		trace("received signal %d (%s), file descriptor %d", sig,
		      strsignal(sig), fd);

		if (sig == SIGTERM || sig == SIGINT)
			break;

		/* Interval tick? */
		if (sig == SIGALRM) {
			sched_poll_expired(bar);
			continue;
		}

		/* Child(ren) dead? */
		if (sig == SIGCHLD) {
			sched_poll_exited(bar);
			bar_dump(bar);
			continue;
		}

		/* Block clicked? */
		if (sig == SIGIO) {
			bar_click(bar);
			continue;
		}

		/* Persistent block ready to be read? */
		if (sig == SIGRTMIN) {
			sched_poll_readable(bar, fd);
			bar_dump(bar);
			continue;
		}

		/* Blocks signaled? */
		if (sig > SIGRTMIN && sig <= SIGRTMAX) {
			sched_poll_signaled(bar, sig - SIGRTMIN);
			continue;
		}

		/* Deprecated signals? */
		if (sig == SIGUSR1 || sig == SIGUSR2) {
			error("SIGUSR{1,2} are deprecated, ignoring.");
			continue;
		}

		debug("unhandled signal %d", sig);
	}

	/* Disable event I/O for blocks (persistent) */
	while (block) {
		if (block->interval == INTERVAL_PERSIST)
			sys_async(block->out[0], 0);

		block = block->next;
	}
	
	/* Disable event I/O for stdin (clicks) */
	sys_async(STDIN_FILENO, 0);

	/*
	 * Unblock signals (so subsequent syscall can be interrupted)
	 * and wait for child processes termination.
	 */
	err = sys_sigunblock(&set);
	if (err)
		error("failed to unblock signals");

	err = sys_waitanychild();
	if (err)
		error("failed to wait any child");

	debug("quit scheduling");
}
