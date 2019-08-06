/*
 * block.h - definition of a block
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

#ifndef BLOCK_H
#define BLOCK_H

#include <sys/types.h>

#include "bar.h"
#include "log.h"
#include "map.h"

#define INTERVAL_ONCE		-1
#define INTERVAL_REPEAT		-2
#define INTERVAL_PERSIST	-3

#define FORMAT_RAW	0
#define FORMAT_JSON	1

/* Block command exit codes */
#define EXIT_URGENT	'!' /* 33 */
#define EXIT_ERR_INTERNAL	66

struct block {
	const struct bar *bar;

	struct map *config;
	struct map *env;

	bool tainted;

	/* Pretty name for log messages */
	char *name;

	/* Shortcuts */
	const char *command;
	int interval;
	int signal;
	unsigned format;

	/* Runtime info */
	unsigned long timestamp;
	int in[2];
	int out[2];
	int code;
	pid_t pid;

	struct block *next;
};

struct block *block_create(struct bar *bar, const struct map *config);
void block_destroy(struct block *block);

int block_reset(struct block *block);

const char *block_get(const struct block *block, const char *key);
int block_set(struct block *block, const char *key, const char *value);

int block_for_each(const struct block *block,
		   int (*func)(const char *key, const char *value, void *data),
		   void *data);

void block_printf(struct block *block, int lvl, const char *fmt, ...);

#define block_fatal(block, fmt, ...) \
	do { \
		fatal("[%s] " fmt, block->name, ##__VA_ARGS__); \
		block_printf(block, LOG_FATAL, "Oops! " fmt, ##__VA_ARGS__); \
	} while (0)

#define block_error(block, fmt, ...) \
	do { \
		error("[%s] " fmt, block->name, ##__VA_ARGS__); \
		block_printf(block, LOG_ERROR, "Error: " fmt, ##__VA_ARGS__); \
	} while (0)

#define block_trace(block, fmt, ...) \
	do { \
		trace("[%s] " fmt, block->name, ##__VA_ARGS__); \
		block_printf(block, LOG_TRACE, fmt, ##__VA_ARGS__); \
	} while (0)

#define block_debug(block, fmt, ...) \
	do { \
		debug("[%s] " fmt, block->name, ##__VA_ARGS__); \
		block_printf(block, LOG_DEBUG, fmt, ##__VA_ARGS__); \
	} while (0)

int block_setup(struct block *block);
int block_click(struct block *block);
int block_spawn(struct block *block);
void block_touch(struct block *block);
int block_reap(struct block *block);
int block_update(struct block *block);
void block_close(struct block *block);

#endif /* BLOCK_H */
