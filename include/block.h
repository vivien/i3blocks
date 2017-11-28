/*
 * block.h - definition of a block
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

#ifndef _BLOCK_H
#define _BLOCK_H

#include <sys/types.h>

#include "click.h"
#include "log.h"
#include "map.h"

#define INTER_ONCE	-1
#define INTER_REPEAT	-2
#define INTER_PERSIST	-3

#define FORMAT_PLAIN	0
#define FORMAT_JSON	1

/* Block command exit codes */
#define EXIT_URGENT	'!' /* 33 */
#define EXIT_ERR_INTERNAL	66

struct block {
	const struct map *defaults;
	struct map *customs;

	/* Shortcuts */
	const char *command;
	int interval;
	unsigned signal;
	unsigned format;

	/* Runtime info */
	unsigned long timestamp;
	pid_t pid;
	int out, err;
};

const char *block_get(const struct block *block, const char *key);

int block_for_each(const struct block *block,
		   int (*func)(const char *key, const char *value, void *data),
		   void *data);

#define bdebug(block, msg, ...) \
	debug("[%s] " msg, block_get(block, "name") ? : "unknown", ##__VA_ARGS__)

#define berror(block, msg, ...) \
	error("[%s] " msg, block_get(block, "name") ? : "unknown", ##__VA_ARGS__)

#define berrorx(block, msg, ...) \
	errorx("[%s] " msg, block_get(block, "name") ? : "unknown", ##__VA_ARGS__)

int block_setup(struct block *);
void block_spawn(struct block *, struct click *);
void block_reap(struct block *);
void block_update(struct block *);

#endif /* _BLOCK_H */
