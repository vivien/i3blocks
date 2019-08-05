/*
 * line.c - generic line parser
 * Copyright (C) 2015-2019  Vivien Didelot
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

#include "line.h"
#include "log.h"
#include "sys.h"

/* Read a single character and return a negative error code if none was read */
static int line_getc(int fd, char *c)
{
	return sys_read(fd, c, 1, NULL);
}

/* Read a line including the newline character and return its positive length */
static ssize_t line_gets(int fd, char *buf, size_t size)
{
	size_t len = 0;
	int err;

	for (;;) {
		if (len == size)
			return -ENOSPC;

		err = line_getc(fd, buf + len);
		if (err)
			return err;

		if (buf[len++] == '\n')
			break;
	}

	/* at least 1 */
	return len;
}

/* Read a line excluding the newline character */
static int line_parse(int fd, line_cb_t *cb, size_t num, void *data)
{
	char buf[BUFSIZ];
	ssize_t len;
	int err;

	len = line_gets(fd, buf, sizeof(buf));
	if (len < 0)
		return len;

	/* replace newline with terminating null byte */
	buf[len - 1] = '\0';

	debug("&%d:%.3d: %s", fd, num, buf);

	if (cb) {
		err = cb(buf, num, data);
		if (err)
			return err;
	}

	return 0;
}

/* Read up to count lines excluding their newline character */
int line_read(int fd, size_t count, line_cb_t *cb, void *data)
{
	size_t lines = 0;
	int err;

	while (count--) {
		err = line_parse(fd, cb, lines++, data);
		if (err)
			return err;
	}

	return 0;
}
