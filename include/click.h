/*
 * click.h - definition of click parsing functions
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

#ifndef _CLICK_H
#define _CLICK_H

struct click {
	char *name;
	char *instance;
	char *button;
	char *x;
	char *y;
	char *relative_x;
	char *relative_y;
	char *width;
	char *height;
};

void click_parse(char *, struct click *);

#endif /* _CLICK_H */
