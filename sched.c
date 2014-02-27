/*
 * i3blocks - define blocks for your i3 status line
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
#include <time.h>
#include <unistd.h>

#include "block.h"
#include "json.h"

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
	bool first_time, outdated, signaled, clicked;

	first_time = outdated = signaled = clicked = false;

	if (block->last_update == 0)
		first_time = true;

	if (block->interval) {
		const unsigned long now = time(NULL);
		const unsigned long next_update = block->last_update + block->interval;

		outdated = ((long) (next_update - now)) <= 0;
	}

	if (caughtsig) {
		signaled = caughtsig == block->signal;
		clicked = block->button > 0;
	}

	return first_time || outdated || signaled || clicked;
}

static void
update_status_line(struct status_line *status)
{
	int i;

	for (i = 0; i < status->num; ++i) {
		const struct block *config_block = status->blocks + i;
		struct block *updated_block = status->updated_blocks + i;

		/* Skip static block */
		if (!*config_block->command)
			continue;

		/* If a block needs an update, reset and execute it */
		if (need_update(updated_block)) {
			const unsigned button = updated_block->button;
			const unsigned x = updated_block->x;
			const unsigned y = updated_block->y;

			memcpy(updated_block, config_block, sizeof(struct block));

			updated_block->button = button;
			updated_block->x = x;
			updated_block->y = y;

			if (block_update(updated_block))
				fprintf(stderr, "failed to update block %s\n", updated_block->name);

			if (updated_block->button)
				updated_block->button = updated_block->x = updated_block->y = 0;
		}
	}

	if (caughtsig > 0)
		caughtsig = 0;
}

/*
 * Parse a click, previous read from stdin.
 *
 * A click looks like this (note that "name" and "instance" are optional):
 *
 *     ',{"name":"foo","instance":"bar","button":1,"x":1186,"y":13}\n'
 */
static void
parse_click(char *click, const char **name, const char **instance,
		unsigned *button, unsigned *x, unsigned *y)
{
#define ATOI(_key) \
	*_key = atoi(strstr(click, "\"" #_key "\":") + strlen("\"" #_key "\":"))

	ATOI(button);
	ATOI(x);
	ATOI(y);

#undef ATOI

#define STR(_key) \
	*_key = strstr(click, "\"" #_key "\":\""); \
	if (*_key) { \
		*_key += strlen("\"" #_key "\":\""); \
		*strchr(*_key, '"') = '\0'; \
	}

	STR(instance);
	STR(name);

#undef STR
}

static void
handle_click(struct status_line *status)
{
	char click[1024];

	const char *name, *instance;
	unsigned button, x, y;

	memset(click, 0, sizeof(click));
	fread(click, 1, sizeof(click) - 1, stdin);

	parse_click(click, &name, &instance, &button, &x, &y);

	if (!name)
		name = "\0";
	if (!instance)
		instance = "\0";

	fprintf(stderr, "got a click: name=%s instance=%s button=%d x=%d y=%d\n",
			name, instance, button, x, y);

	/* find the corresponding block */
	if (*name || *instance) {
		int i;

		for (i = 0; i < status->num; ++i) {
			struct block *block = status->updated_blocks + i;

			if (strcmp(block->name, name) == 0 &&
					strcmp(block->instance, instance) == 0) {
				block->button = button;
				block->x = x;
				block->y = y;

				/* It shouldn't be likely to have several blocks with the same name/instance, so stop here */
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
		perror("sigaction");
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
		fprintf(stderr, "failed to set SIGIO\n");
		return 1;
	}

	/* Set owner process that is to receive "I/O possible" signal */
	if (fcntl(STDIN_FILENO, F_SETOWN, getpid()) == -1) {
		fprintf(stderr, "failed to set process as owner for stdin\n");
		return 1;
	}

	/* Enable "I/O possible" signaling and make I/O nonblocking for file descriptor */
	flags = fcntl(STDIN_FILENO, F_GETFL);
	if (fcntl(STDIN_FILENO, F_SETFL, flags | O_ASYNC | O_NONBLOCK) == -1) {
		fprintf(stderr, "failed to enable I/O signaling for stdin\n");
		return 1;
	}

	return 0;
}

int
sched_init(void)
{
	/* Setup signal handler for blocks */
	if (sched_use_signal(SIGUSR1)) {
		fprintf(stderr, "failed to set SIGUSR1\n");
		return 1;
	}

	if (sched_use_signal(SIGUSR2)) {
		fprintf(stderr, "failed to set SIGUSR2\n");
		return 1;
	}

	/* Setup event I/O for stdin (clicks) */
	if (sched_event_stdin()) {
		fprintf(stderr, "failed to setup event I/O for stdin\n");
		return 1;
	}

	return 0;
}

void
sched_start(struct status_line *status)
{
	const unsigned sleeptime = longest_sleep(status);


	while (1) {
		update_status_line(status);
		json_print_status_line(status);

		/* Sleep or force check on interruption */
		if (sleep(sleeptime))
			if (caughtsig == SIGIO)
				handle_click(status);
	}
}
