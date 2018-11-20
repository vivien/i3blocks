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

#ifdef HAVE_CONFIG_H
#include "i3blocks-config.h"
#endif

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "bar.h"
#include "log.h"

log_handle_t log_handle = NULL;
int log_level = LOG_FATAL;
void *log_data = NULL;

int
main(int argc, char *argv[])
{
	char *path = NULL;
	struct bar *bar;
	int c;

	while (c = getopt(argc, argv, "c:vhV"), c != -1) {
		switch (c) {
		case 'c':
			path = optarg;
			break;
		case 'v':
			log_level++;
			break;
		case 'h':
			printf("Usage: %s [-c <configfile>] [-v] [-h] [-V]\n", argv[0]);
			return 0;
		case 'V':
			printf(PACKAGE_STRING " © 2014 Vivien Didelot and contributors\n");
			return 0;
		default:
			error("Try '%s -h' for more information.", argv[0]);
			return 1;
		}
	}

	bar = bar_create();
	if (!bar)
		return EXIT_FAILURE;

	bar_load(bar, path);

	bar_schedule(bar);

	bar_destroy(bar);

	return EXIT_SUCCESS;
}
