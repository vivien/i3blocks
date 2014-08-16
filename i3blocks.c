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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "block.h"
#include "ini.h"
#include "json.h"
#include "log.h"
#include "sched.h"

#ifndef VERSION
#define VERSION "unknown"
#endif

#ifndef BLOCK_LIBEXEC
#define BLOCK_LIBEXEC "/usr/local/libexec/i3blocks/"
#endif

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
	char *libexec = BLOCK_LIBEXEC;
	struct bar *bar;
	int c;

	while (c = getopt(argc, argv, "c:d:hv"), c != -1) {
		switch (c) {
		case 'c':
			inifile = optarg;
			break;
		case 'd':
			libexec = optarg;
			break;
		case 'h':
			printf("Usage: %s [-c <configfile>] [-d <scriptsdir>] [-h] [-v]\n", argv[0]);
			return 0;
		case 'v':
			printf("i3blocks " VERSION " © 2014 Vivien Didelot and contributors\n");
			return 0;
		default:
			error("Try '%s -h' for more information.", argv[0]);
			return 1;
		}
	}

	if (setenv("BLOCK_LIBEXEC", libexec, 1) == -1) {
		errorx("setenv BLOCK_LIBEXEC");
		return 1;
	}
	debug("$BLOCK_LIBEXEC set to '%s'", libexec);

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
