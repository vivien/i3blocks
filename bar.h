/*
 * bar.h - definition of a status line handling functions
 * Copyright (C) 2014-2019  Vivien Didelot
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

#ifndef BAR_H
#define BAR_H

#include <stdbool.h>

#include "block.h"
#include "sys.h"

struct bar {
	struct block *blocks;
	sigset_t sigset;
	bool term;
};

#define bar_printf(bar, lvl, fmt, ...) \
	block_printf(bar->blocks, lvl, "%s" fmt, bar->term ? "TTY " : "", ##__VA_ARGS__)

#define bar_fatal(bar, fmt, ...) \
	do { \
		fatal(fmt, ##__VA_ARGS__); \
		bar_printf(bar, LOG_FATAL, "Oops! " fmt ". Increase log level for details.", ##__VA_ARGS__); \
	} while (0)

#define bar_error(bar, fmt, ...) \
	do { \
		error(fmt, ##__VA_ARGS__); \
		bar_printf(bar, LOG_ERROR, "Error: " fmt, ##__VA_ARGS__); \
	} while (0)

#define bar_trace(bar, fmt, ...) \
	do { \
		trace(fmt, ##__VA_ARGS__); \
		bar_printf(bar, LOG_TRACE, "Trace: " fmt, ##__VA_ARGS__); \
	} while (0)

#define bar_debug(bar, fmt, ...) \
	do { \
		debug(fmt, ##__VA_ARGS__); \
		bar_printf(bar, LOG_DEBUG, "Debug: " fmt, ##__VA_ARGS__); \
	} while (0)

int bar_init(bool term, const char *path);

struct map;

/* i3bar.c */
int i3bar_read(int fd, size_t count, struct map *map);
int i3bar_click(struct bar *bar);
int i3bar_print(const struct bar *bar);
int i3bar_printf(struct block *block, int lvl, const char *msg);
int i3bar_setup(struct block *block);
int i3bar_start(struct bar *bar);
void i3bar_stop(struct bar *bar);

#endif /* BAR_H */
