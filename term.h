/*
 * term.h - terminal output handling functions
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

#ifndef TERM_H
#define TERM_H

#include <stdio.h>

static inline void term_save_cursor(void)
{
	fprintf(stdout, "\033[s\033[?25l");
}

static inline void term_restore_cursor(void)
{
	fprintf(stdout, "\033[u\033[K");
}

static inline void term_reset_cursor(void)
{
	fprintf(stdout, "\033[?25h");
}

#endif /* TERM_H */
