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
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "block.h"
#include "ini.h"
#include "json.h"

#ifndef VERSION
#define VERSION "unknown"
#endif

volatile sig_atomic_t caughtsig = 0;

void
handler(int signum)
{
	caughtsig = signum;
}

static void
start(void)
{
	fprintf(stdout, "{\"version\":1,\"click_events\":true}\n[[]\n");
	fflush(stdout);
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
	fprintf(stderr, "parsing click: %s\n", click);

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

int
main(int argc, char *argv[])
{
	char *inifile = NULL;
	struct sigaction sa;
	struct status_line *status;
	int c;
	int flags;

	while (c = getopt(argc, argv, "c:hv"), c != -1) {
		switch (c) {
		case 'c':
			inifile = optarg;
			break;
		case 'h':
			printf("Usage: %s [-c <configfile>] [-h] [-v]\n", argv[0]);
			return 0;
		case 'v':
			printf("i3blocks " VERSION " © 2014 Vivien Didelot and contributors\n");
			return 0;
		default:
			fprintf(stderr, "Try '%s -h' for more information.\n", argv[0]);
			return 1;
		}
	}

	status = load_status_line(inifile);
	if (!status) {
		fprintf(stderr, "Try '%s -h' for more information.\n", argv[0]);
		return 1;
	}

	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART; /* Restart functions if interrupted by handler */

	/* Setup signal handler for blocks */
	if (sigaction(SIGUSR1, &sa, NULL) == -1 || sigaction(SIGUSR2, &sa, NULL) == -1) {
		fprintf(stderr, "failed to setup a signal handler\n");
		return 1;
	}

	/* Setup signal handler for stdin */
	if (sigaction(SIGIO, &sa, NULL) == -1) {
		fprintf(stderr, "failed to setup a SIGIO handler for stdin\n");
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

	start();

	while (1) {
		update_status_line(status);
		print_status_line(status);

		/* Sleep or force check on interruption */
		if (sleep(status->sleeptime))
			if (caughtsig == SIGIO)
				handle_click(status);
	}

	//stop();
	return 0;
}
