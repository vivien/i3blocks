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

#define PROP_I3BAR	1 /* See http://i3wm.org/docs/i3bar-protocol.html */
#define PROP_STRING	2
#define PROP_NUMBER	4
#define PROP_BOOLEAN	8

#define INTER_ONCE	-1
#define INTER_REPEAT	-2
#define INTER_PERSIST	-3

#define FORMAT_PLAIN	0
#define FORMAT_JSON	1

/* Block command exit codes */
#define EXIT_URGENT	'!' /* 33 */
#define EXIT_ERR_INTERNAL	66

#define PROPERTIES(_) \
	_(full_text,             1024, PROP_I3BAR | PROP_STRING) \
	_(short_text,            512,  PROP_I3BAR | PROP_STRING) \
	_(color,                 8,    PROP_I3BAR | PROP_STRING) \
	_(min_width,             1024, PROP_I3BAR | PROP_STRING | PROP_NUMBER) \
	_(align,                 8,    PROP_I3BAR | PROP_STRING) \
	_(name,                  32,   PROP_I3BAR | PROP_STRING) \
	_(instance,              32,   PROP_I3BAR | PROP_STRING) \
	_(urgent,                8,    PROP_I3BAR | PROP_BOOLEAN) \
	_(separator,             8,    PROP_I3BAR | PROP_BOOLEAN) \
	_(separator_block_width, 8,    PROP_I3BAR | PROP_NUMBER) \
	_(command,               1024,              PROP_STRING) \
	_(interval,              8,                 PROP_STRING | PROP_NUMBER) \
	_(signal,                8,                 PROP_NUMBER) \
	_(label,                 32,                PROP_STRING) \
	_(format,                8,                 PROP_STRING | PROP_NUMBER) \

struct properties {
#define DEFINE(_name, _size, _flags) char _name[_size];
	PROPERTIES(DEFINE);
#undef DEFINE
};

struct block {
	struct properties default_props;
	struct properties updated_props;

	/* Shortcuts */
	int interval;
	unsigned signal;
	unsigned format;

	/* Runtime info */
	unsigned long timestamp;
	pid_t pid;
	int out, err;
};

/* Shortcuts to config */
#define NAME(_block)		(_block->default_props.name)
#define INSTANCE(_block)	(_block->default_props.instance)
#define COMMAND(_block)		(_block->default_props.command)
#define LABEL(_block)		(_block->default_props.label)

/* Shortcuts to update */
#define FULL_TEXT(_block)	(_block->updated_props.full_text)

#define bdebug(block, msg, ...) \
	debug("[%s] " msg, NAME(block), ##__VA_ARGS__)

#define berror(block, msg, ...) \
	error("[%s] " msg, NAME(block), ##__VA_ARGS__)

#define berrorx(block, msg, ...) \
	errorx("[%s] " msg, NAME(block), ##__VA_ARGS__)

void block_setup(struct block *);
void block_spawn(struct block *, struct click *);
void block_reap(struct block *);
void block_update(struct block *);

#endif /* _BLOCK_H */
