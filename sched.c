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
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "block.h"
#include "json.h"
#include "log.h"
#include "sched.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SIGNAL() pthread_cond_signal(&cond)

static pthread_cond_t cond;
static volatile sig_atomic_t caughtsig;

static void
handler(int signum)
{
	caughtsig = signum;
}

static unsigned int
longest_sleep(struct status_line *status)
{
	int time = 0;

	/* The maximum sleep time is actually the GCD between all block intervals */
	int gcd(int a, int b) {
		while (b != 0)
			a %= b, a ^= b, b ^= a, a ^= b;

		return a;
	}

	if (status->num > 0) {
		time = status->blocks->interval; /* first block's interval */

		if (status->num >= 2) {
			int i;

			for (i = 1; i < status->num; ++i)
				time = gcd(time, (status->blocks + i)->interval);
		}
	}

	return time > 0 ? time : 5; /* default */
}

static inline bool
need_update(struct block *block)
{
	bool first_time, outdated, signaled, clicked, wait_ended;

	first_time = outdated = signaled = clicked = wait_ended = false;

	if (block->last_update == 0)
		first_time = true;

	if (block->interval) {
		const unsigned long now = time(NULL);
		const unsigned long next_update = block->last_update + block->interval;

		outdated = ((long) (next_update - now)) <= 0;
	}

	if (caughtsig) {
		signaled = caughtsig == block->signal;
		clicked = *block->click.button != '\0';
	}

	wait_ended = *block->wait_command && !block->waiting;

	bdebug(block, "CHECK first_time: %s, outdated: %s, signaled: %s, clicked: %s, wait_ended: %s",
			first_time ? "YES" : "no",
			outdated ? "YES" : "no",
			signaled ? "YES" : "no",
			wait_ended ? "YES" : "no",
			clicked ? "YES" : "no");

	return first_time || outdated || signaled || clicked || wait_ended;
}

static void *
wait_thread(void *arg)
{
	struct block *block = (struct block *) arg;
	block_update_wait(block);
	block->waiting = false;
	SIGNAL();
	return NULL;
}

static void
update_block(struct block *block)
{
	if (!*block->wait_command)
		return block_update(block);

	/* Exec the simple command the first time */
	if (!block->last_update)
		block_update(block);

	/* Start the thread of a wait command */
	pthread_t thread;
	block->waiting = true;
	pthread_create(&thread, NULL, wait_thread, (void *) block);
}

static bool
update_status_line(struct status_line *status)
{
	int i;
	bool changed = false;

	for (i = 0; i < status->num; ++i) {
		const struct block *config_block = status->blocks + i;
		struct block *updated_block = status->updated_blocks + i;

		/* Skip static block */
		if (!*config_block->command && !*config_block->wait_command) {
			bdebug(config_block, "no command, skipping");
			continue;
		}

		/* If a block needs an update, execute it */
		if (need_update(updated_block)) {
			update_block(updated_block);
			changed = true;
		}
	}

	if (caughtsig > 0)
		caughtsig = 0;

	return changed;
}

/*
 * Parse a click, previous read from stdin.
 *
 * A click looks like this ("name" and "instance" can be absent):
 *
 *     ',{"name":"foo","instance":"bar","button":1,"x":1186,"y":13}\n'
 *
 * Note that this function is non-idempotent. We need to parse from right to
 * left. It's ok since the JSON layout is known and fixed.
 */
static void
parse_click(char *json, char **name, char **instance, struct click *click)
{
	int nst, nlen;
	int ist, ilen;
	int bst, blen;
	int xst, xlen;
	int yst, ylen;

	json_parse(json, "y", &yst, &ylen);
	json_parse(json, "x", &xst, &xlen);
	json_parse(json, "button", &bst, &blen);
	json_parse(json, "instance", &ist, &ilen);
	json_parse(json, "name", &nst, &nlen);

	/* set name, otherwise "" */
	*name = (json + nst);
	*(*name + nlen) = '\0';

	/* set instance, otherwise "" */
	*instance = (json + ist);
	*(*instance + ilen) = '\0';

	memcpy(click->button, json + bst, MIN(blen, sizeof(click->button) - 1));
	memcpy(click->x, json + xst, MIN(xlen, sizeof(click->x) - 1));
	memcpy(click->y, json + yst, MIN(ylen, sizeof(click->y) - 1));
}

static void
handle_click(struct status_line *status)
{
	char json[1024] = { 0 };
	struct click click = { "" };
	char *name, *instance;

	fread(json, 1, sizeof(json) - 1, stdin);

	parse_click(json, &name, &instance, &click);
	debug("got a click: name=%s instance=%s button=%s x=%s y=%s",
			name, instance, click.button, click.x, click.y);

	/* find the corresponding block */
	if (*name || *instance) {
		int i;

		for (i = 0; i < status->num; ++i) {
			struct block *block = status->updated_blocks + i;

			if (strcmp(block->name, name) == 0 && strcmp(block->instance, instance) == 0) {
				memcpy(&block->click, &click, sizeof(struct click));

				/* It shouldn't be likely to have several blocks with the same name/instance, so stop here */
				bdebug(block, "clicked");
				break;
			}
		}
	}
}

static int
sched_use_signal(const int sig)
{
	struct sigaction sa;

	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART; /* Restart functions if interrupted by handler */

	if (sigaction(sig, &sa, NULL) == -1) {
		errorx("sigaction");
		return 1;
	}

	return 0;
}

static int
sched_event_stdin(void)
{
	int flags;

	/* Setup signal handler for stdin */
	if (sched_use_signal(SIGIO)) {
		error("failed to set SIGIO");
		return 1;
	}

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

static void *
update_thread(void *arg)
{
	pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

	struct status_line *status = (struct status_line *) arg;
	const unsigned sleeptime = longest_sleep(status);

	struct timeval now;
	struct timespec timeout;

	debug("starting scheduler with sleep time %d", sleeptime);

	pthread_mutex_lock(&mut);

	while (1) {
		/* If at least a block has changed */
		if (update_status_line(status))
			json_print_status_line(status);

		gettimeofday(&now, NULL);

		timeout.tv_sec = now.tv_sec + sleeptime;
		timeout.tv_nsec = now.tv_usec * 1000;

		pthread_cond_timedwait(&cond, &mut, &timeout);
	}

	pthread_mutex_unlock(&mut);

	return NULL;
}

int
sched_init(void)
{
	/* Setup signal handler for blocks */
	if (sched_use_signal(SIGUSR1)) {
		error("failed to set SIGUSR1");
		return 1;
	}

	if (sched_use_signal(SIGUSR2)) {
		error("failed to set SIGUSR2");
		return 1;
	}

	/* Setup event I/O for stdin (clicks) */
	if (!isatty(STDIN_FILENO)) {
		if (sched_event_stdin()) {
			error("failed to setup event I/O for stdin");
			return 1;
		}
	}

	return 0;
}

void
sched_start(struct status_line *status)
{
	pthread_cond_t new_cond = PTHREAD_COND_INITIALIZER;
	cond = new_cond;
	pthread_t thread;

	pthread_create(&thread, NULL, update_thread, (void *) status);

	while (1) {
		/* Sleep and force check on interruption */
		if (sleep(UINT_MAX)) {
			debug("woken up by signal %d", caughtsig);
			if (caughtsig == SIGIO) {
				debug("stdin readable");
				handle_click(status);
			}

			/* Force thread refresh */
			SIGNAL();
		}
	}
}
