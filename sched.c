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
	unsigned int time = 0;

	if (bar->num > 0 && bar->blocks->interval > 0)
		time = bar->blocks->interval; /* first block's interval */

	if (bar->num < 2)
		return time;

	/* The maximum sleep time is actually the GCD between all block intervals */
	for (int i = 1; i < bar->num; ++i)
		if ((bar->blocks + i)->interval > 0)
			time = gcd(time, (bar->blocks + i)->interval);

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

void
sched_start(struct bar *bar)
{
	int sig, fd;
	int err;

	/* First forks (for commands with an interval) */
	bar_poll_timed(bar);

	while (1) {
		err = sys_sigwaitinfo(&set, &sig, &fd);
		if (err) {
			/* Hiding the bar may interrupt this system call */
			if (err == -EINTR)
				continue;
			break;
		}

		debug("received signal %d (%s)", sig, strsignal(sig));

		if (sig == SIGTERM || sig == SIGINT)
			break;

		/* Interval tick? */
		if (sig == SIGALRM) {
			bar_poll_outdated(bar);

		/* Child(ren) dead? */
		} else if (sig == SIGCHLD) {
			bar_poll_exited(bar);
			bar_dump(bar);

		/* Block clicked? */
		} else if (sig == SIGIO) {
			bar_click(bar);

		/* Persistent block ready to be read? */
		} else if (sig == SIGRTMIN) {
			bar_poll_readable(bar, fd);
			bar_dump(bar);

		/* Blocks signaled? */
		} else if (sig > SIGRTMIN && sig <= SIGRTMAX) {
			bar_poll_signaled(bar, sig - SIGRTMIN);

		/* Deprecated signals? */
		} else if (sig == SIGUSR1 || sig == SIGUSR2) {
			error("SIGUSR{1,2} are deprecated, ignoring.");

		} else debug("unhandled signal %d", sig);
	}

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
