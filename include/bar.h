/*
 * bar.h - definition of a status line handling functions
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

#ifndef _BAR_H
#define _BAR_H

struct bar {
	struct block *blocks;
	unsigned int num;
};

void bar_poll_timed(struct bar *);
void bar_poll_clicked(struct bar *);
void bar_poll_outdated(struct bar *);
void bar_poll_signaled(struct bar *, int);
void bar_poll_exited(struct bar *);
void bar_poll_readable(struct bar *, int);

#endif /* _BAR_H */
