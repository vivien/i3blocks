/*
 * log.h - debug and error printing macros
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

#ifndef _LOG_H
#define _LOG_H

#include <errno.h>
#include <stdio.h>
#include <string.h>

extern unsigned log_level;

enum log_level {
	LOG_NORMAL,
	LOG_WARN,
	LOG_DEBUG,
};

#define debug(msg, ...) \
	if (log_level >= LOG_DEBUG) { \
		fprintf(stderr, "DEBUG %s:%d: " msg "\n", __func__, __LINE__, ##__VA_ARGS__); \
	}

#define error(msg, ...) \
	fprintf(stderr, "ERROR %s:%d: " msg "\n", __func__, __LINE__, ##__VA_ARGS__)

#define errorx(msg, ...) \
	error(msg ": %s", ##__VA_ARGS__, strerror(errno))

#endif /* _LOG_H */
