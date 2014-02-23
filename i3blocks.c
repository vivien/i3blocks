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

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
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
	fprintf(stdout, "{\"version\":1}\n[[]\n");
	fflush(stdout);
}

int
main(int argc, char *argv[])
{
	char *inifile = NULL;
	struct sigaction sa;
	struct status_line *status;
	int c;

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

	if (sigaction(SIGUSR1, &sa, NULL) == -1 || sigaction(SIGUSR2, &sa, NULL) == -1) {
		fprintf(stderr, "failed to setup a signal handler\n");
		return 1;
	}

	start();

	while (1) {
		update_status_line(status);
		print_status_line(status);

		/* Sleep or force check on interruption */
		sleep(status->sleeptime);
	}

	//stop();
	return 0;
}
