/*
 * i3blocks.c - main entry point, load the config and start the scheduler
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
#include <stdio.h>

#include "ini.h"
#include "log.h"
#include "sched.h"

#ifndef VERSION
#define VERSION "unknown"
#endif

unsigned log_level = LOG_NORMAL;

static void
start(void)
{
	fprintf(stdout, "{\"version\":1,\"click_events\":true}\n[[]\n");
	fflush(stdout);
}

int
main(int argc, char *argv[])
{
	char *inifile = NULL;
	struct bar *bar;
	int c;

	while (c = getopt(argc, argv, "c:vhV"), c != -1) {
		switch (c) {
		case 'c':
			inifile = optarg;
			break;
		case 'v':
			log_level++;
			break;
		case 'h':
			printf("Usage: %s [-c <configfile>] [-h] [-V]\n", argv[0]);
			return 0;
		case 'V':
			printf("i3blocks " VERSION " © 2014 Vivien Didelot and contributors\n");
			return 0;
		default:
			error("Try '%s -h' for more information.", argv[0]);
			return 1;
		}
	}

	debug("log level %u", log_level);

	bar = ini_load(inifile);
	if (!bar) {
		error("Try '%s -h' for more information.", argv[0]);
		return 1;
	}

	if (sched_init(bar))
		return 1;

	start();
	sched_start(bar);

	//stop();
	return 0;
}
