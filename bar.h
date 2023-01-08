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

int bar_init(bool term, const char *path);

struct map;

/* i3bar.c */
int i3bar_read(int fd, size_t count, struct map *map);
int i3bar_click(struct block *bar);
int i3bar_print(const struct block *bar);
int i3bar_printf(struct block *block, int lvl, const char *msg);
int i3bar_setup(struct block *block);
int i3bar_start(struct block *bar);
void i3bar_stop(struct block *bar);

#endif /* BAR_H */
