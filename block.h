/*
 * i3blocks - define blocks for your i3bar status line
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

/* Keys part of the i3bar protocol */
#define PROTOCOL_KEYS(_) \
	_(full_text,             1024, string) \
	_(short_text,            512,  string) \
	_(color,                 8,    string) \
	_(min_width,             1024, string_or_number) \
	_(align,                 8,    string) \
	_(name,                  32,   string) \
	_(instance,              32,   string) \
	_(urgent,                8,    boolean) \
	_(separator,             8,    boolean) \
	_(separator_block_width, 8,    number) \

/* click event */
struct click {
	char button[2];
	char x[5];
	char y[5];
};

struct block {
#define MEMBER(_name, _size, _type) char _name[_size];

	PROTOCOL_KEYS(MEMBER);
	MEMBER(command, 1024, string);
	unsigned interval;
	unsigned signal;
	unsigned long last_update;
	struct click click;

#undef MEMBER
};

struct status_line {
	struct block *blocks;
	struct block *updated_blocks;
	unsigned int num;
};

void block_update(struct block *);

#endif /* _BLOCK_H */
