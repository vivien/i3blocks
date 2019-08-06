/*
 * log.h - syslog friendly error and debug printing macros
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

#ifndef LOG_H
#define LOG_H

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define SYSLOG_CRIT	"<2>"
#define SYSLOG_ERR	"<3>"
#define SYSLOG_NOTICE	"<5>"
#define SYSLOG_DEBUG	"<7>"

extern enum {
	LOG_FATAL,
	LOG_ERROR,
	LOG_TRACE,
	LOG_DEBUG,
} log_level;

static inline void log_printf(int lvl, const char *fmt, ...)
{
	va_list ap;

	if (lvl <= LOG_ERROR || lvl <= log_level) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}
}

#define fatal(fmt, ...) \
	log_printf(LOG_FATAL, SYSLOG_CRIT fmt "\n", ##__VA_ARGS__)

#define error(fmt, ...) \
	log_printf(LOG_ERROR, SYSLOG_ERR fmt "\n", ##__VA_ARGS__)

#define trace(fmt, ...) \
	log_printf(LOG_TRACE, SYSLOG_NOTICE fmt "\n", ##__VA_ARGS__)

#define debug(fmt, ...) \
	log_printf(LOG_DEBUG, SYSLOG_DEBUG "%s:%s:%d: " fmt "\n", \
		   __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#endif /* LOG_H */
