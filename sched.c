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

#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "bar.h"
#include "block.h"
#include "click.h"
#include "json.h"
#include "log.h"

static sigset_t sigset;

static unsigned int
longest_sleep(struct bar *bar)
{
	unsigned int time = 0;

	/* The maximum sleep time is actually the GCD between all block intervals */
	int gcd(int a, int b) {
		while (b != 0)
			a %= b, a ^= b, b ^= a, a ^= b;

		return a;
	}

	if (bar->num > 0 && bar->blocks->interval > 0)
		time = bar->blocks->interval; /* first block's interval */

	if (bar->num < 2)
		return time;

	for (int i = 1; i < bar->num; ++i)
		if ((bar->blocks + i)->interval > 0)
			time = gcd(time, (bar->blocks + i)->interval);

	return time;
}

static int
setup_timer(struct bar *bar)
{
	const unsigned sleeptime = longest_sleep(bar);

	if (!sleeptime) {
		debug("no timer needed");
		return 0;
	}

	struct itimerval itv = {
		.it_value.tv_sec = sleeptime,
		.it_interval.tv_sec = sleeptime,
	};

	if (setitimer(ITIMER_REAL, &itv, NULL) == -1) {
		errorx("setitimer");
		return 1;
	}

	debug("starting timer with interval of %d seconds", sleeptime);
	return 0;
}

static int
setup_signals(void)
{
	if (sigemptyset(&sigset) == -1) {
		errorx("sigemptyset");
		return 1;
	}

#define ADD_SIG(_sig) \
	if (sigaddset(&sigset, _sig) == -1) { errorx("sigaddset(%d)", _sig); return 1; }

	/* Control signals */
	ADD_SIG(SIGTERM);
	ADD_SIG(SIGINT);

	/* Timer signal */
	ADD_SIG(SIGALRM);

	/* Block updates (forks) */
	ADD_SIG(SIGCHLD);

	/* Deprecated signals */
	ADD_SIG(SIGUSR1);
	ADD_SIG(SIGUSR2);

	/* Click signal */
	ADD_SIG(SIGIO);

	/* Real-time signals for blocks */
	for (int sig = SIGRTMIN + 1; sig <= SIGRTMAX; ++sig) {
		debug("provide signal %d (%s)", sig, strsignal(sig));
		ADD_SIG(sig);
	}

#undef ADD_SIG

	/* Block signals for which we are interested in waiting */
	if (sigprocmask(SIG_SETMASK, &sigset, NULL) == -1) {
		errorx("sigprocmask");
		return 1;
	}

	return 0;
}

static int
eventio_stdin(void)
{
	int flags;

	/* Set owner process that is to receive "I/O possible" signal */
	if (fcntl(STDIN_FILENO, F_SETOWN, getpid()) == -1) {
		error("failed to set process as owner for stdin");
		return 1;
	}

	/* Enable "I/O possible" signaling and make I/O nonblocking for file descriptor */
	flags = fcntl(STDIN_FILENO, F_GETFL);
	if (fcntl(STDIN_FILENO, F_SETFL, flags | O_ASYNC | O_NONBLOCK) == -1) {
		error("failed to enable I/O signaling for stdin");
		return 1;
	}

	return 0;
}

int
sched_init(struct bar *bar)
{
	if (setup_signals())
		return 1;

	if (setup_timer(bar))
		return 1;

	/* Setup event I/O for stdin (clicks) */
	if (!isatty(STDIN_FILENO))
		if (eventio_stdin())
			return 1;

	return 0;
}

void
sched_start(struct bar *bar)
{
	int sig;

	/*
	 * Initial display (for static blocks and loading labels),
	 * and first forks (for commands with an interval).
	 */
	json_print_bar(bar);
	bar_poll_timed(bar);

	while (1) {
		sig = sigwaitinfo(&sigset, NULL);
		if (sig == -1) {
			error("sigwaitinfo");
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
			json_print_bar(bar);

		/* Block clicked? */
		} else if (sig == SIGIO) {
			bar_poll_clicked(bar);

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
	if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) == -1)
		errorx("sigprocmask");
	while (waitpid(-1, NULL, 0) > 0)
		continue;

	debug("quit scheduling");
}
