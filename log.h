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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef void (*log_handle_t)(int lvl, const char *fmt, ...);

enum {
	LOG_FATAL,
	LOG_ERROR,
	LOG_DEBUG,
	LOG_TRACE,
};

extern log_handle_t log_handle;
extern void *log_data;
extern int log_level;

static inline void log_printf(int lvl, const char *fmt, ...)
{
	va_list ap;

	if (log_level < lvl)
		return;

	fprintf(stderr, "<%d> ", lvl);

	switch (lvl) {
	case LOG_FATAL:
		fprintf(stderr, "FATAL ");
		break;
	case LOG_ERROR:
		fprintf(stderr, "ERROR ");
		break;
	case LOG_DEBUG:
		fprintf(stderr, "DEBUG ");
		break;
	case LOG_TRACE:
		fprintf(stderr, "TRACE ");
		break;
	}

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

#define log(lvl, fmt, ...)					\
	{							\
		log_printf(lvl, "%s:%s:%d: " fmt "\n",		\
			   __FILE__, __func__, __LINE__,	\
			   ##__VA_ARGS__);			\
								\
		if (log_handle)					\
			log_handle(lvl, fmt, ##__VA_ARGS__);	\
	}

#define fatal(...) \
	log(LOG_FATAL, ##__VA_ARGS__)

#define error(...) \
	log(LOG_ERROR, ##__VA_ARGS__)

#define debug(...) \
	log(LOG_DEBUG, ##__VA_ARGS__)

#define trace(...) \
	log(LOG_TRACE, ##__VA_ARGS__)

#endif /* _LOG_H */
