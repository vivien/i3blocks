/*
 * i3blocks - simple i3 status line
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

#include <stdbool.h>

/* Keys part of the i3bar protocol */
#define PROTOCOL_KEYS(_) \
	_(full_text,             string) \
	_(short_text,            string) \
	_(color,                 string) \
	_(min_width,             string_or_number) \
	_(align,                 string) \
	_(name,                  string) \
	_(instance,              string) \
	_(urgent,                boolean) \
	_(separator,             boolean) \
	_(separator_block_width, number) \

struct block {
#define MEMBER(_name, _type) char *_name;

	PROTOCOL_KEYS(MEMBER)

	char *command;
	unsigned interval;
	unsigned long last_update;
};

void init_block(struct block *);
int update_block(struct block *);
inline int need_update(struct block *);
void free_block(struct block *);

#endif /* _BLOCK_H */
