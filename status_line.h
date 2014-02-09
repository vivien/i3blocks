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

#ifndef _STATUS_LINE_H
#define _STATUS_LINE_H

struct status_line {
	struct block *blocks;
	unsigned int num;
	unsigned int sleeptime;
};

struct status_line *load_status_line(const char *);
void update_status_line(struct status_line *);
void print_status_line(struct status_line *);
void mark_update(struct status_line *);

#endif /* _STATUS_LINE_H */
