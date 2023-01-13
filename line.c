/*
 * line.c - strict line parsing code
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

/* Read a line excluding the newline character */
static int line_parse(int fd, line_cb_t *cb, size_t num, void *data)
{
	char buf[BUFSIZ];
	size_t len;
	int err;

	err = sys_getline(fd, buf, sizeof(buf), &len);
	if (err)
		return err;

	if (buf[len - 1] != '\n') {
		debug("&%d:%.3d: %.*s\\ No newline", fd, num, len, buf);
		return -EINVAL;
	}

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
