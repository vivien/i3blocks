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

struct block {
	/* Keys part of the i3bar protocol */
	char *full_text;
	char *short_text;
	char *color;
	char *min_width;
	char *align;
	char *name;
	char *instance;
	bool urgent;
	bool separator;
	unsigned separator_block_width;

	/* Keys used by i3blocks */
	char *command;
	unsigned interval;
	unsigned timeout;
	unsigned long last_update;
};

void init_block(struct block *);
void block_to_json(struct block *);
int update_block(struct block *);
inline int need_update(struct block *);
void free_block(struct block *);

#endif /* _BLOCK_H */
