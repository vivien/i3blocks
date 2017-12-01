/*
 * io.c - non-blocking I/O operations
 * Copyright (C) 2015  Vivien Didelot
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

#define _GNU_SOURCE /* for F_SETSIG */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "io.h"
#include "log.h"

/* Open a file read-only and nonblocking */
int io_open(const char *path)
{
	int err;
	int fd;

	fd = open(path, O_RDONLY | O_NONBLOCK);
	if (fd == -1) {
		err = -errno;
		if (err == -ENOENT) {
			debug("open(\"%s\"): No such file or directory", path);
		} else {
			errorx("open(\"%s\")", path);
		}

		return err;
	}

	return fd;
}

/* Close a file */
int io_close(int fd)
{
	int err;

	err = close(fd);
	if (err == -1) {
		err = -errno;
		errorx("close(%d)", fd);
		return err;
	}

	return 0;
}

/* Bind a signal to a file */
int io_signal(int fd, int sig)
{
	int flags;

	/* Assign the signal for this file descriptor */
	if (fcntl(fd, F_SETSIG, sig) == -1) {
		errorx("failed to set signal %d on fd %d", sig, fd);
		return 1;
	}

	/* Set owner process that is to receive "I/O possible" signal */
	if (fcntl(fd, F_SETOWN, getpid()) == -1) {
		errorx("failed to set process as owner of fd %d", fd);
		return 1;
	}

	flags = fcntl(fd, F_GETFL);
	if (flags == -1) {
		errorx("failed to get flags of fd %d", fd);
		return 1;
	}

	/* Enable "I/O possible" signaling and make I/O nonblocking */
	if (fcntl(fd, F_SETFL, flags | O_ASYNC | O_NONBLOCK) == -1) {
		errorx("failed to enable I/O signaling for fd %d", fd);
		return 1;
	}

	return 0;
}

/* Read up to size bytes and return the positive count on success */
static ssize_t io_read(int fd, char *buf, size_t size)
{
	ssize_t nr;
	int err;

	nr = read(fd, buf, size);
	if (nr == -1) {
		err = -errno;
		if (err == -EAGAIN || err == -EWOULDBLOCK) {
			debug("read(%d): would block", fd);
			return -EAGAIN;
		} else {
			errorx("read(%d)", fd);
			return err;
		}
	}

	if (nr == 0) {
		debug("read(%d): end-of-pipe", fd);
		return -EPIPE;
	}

	return nr;
}

/* Read a single character and return a negative error code if none was read */
static int io_getchar(int fd, char *c)
{
	ssize_t nr;

	nr = io_read(fd, c, 1);
	if (nr < 0)
		return nr;

	return 0;
}

/* Read a line including the newline character and return its positive length */
static ssize_t io_getline(int fd, char *buf, size_t size)
{
	size_t len = 0;
	int err;

	for (;;) {
		if (len == size)
			return -ENOSPC;

		err = io_getchar(fd, buf + len);
		if (err)
			return err;

		if (buf[len++] == '\n')
			break;
	}

	/* at least 1 */
	return len;
}

/* Read a line excluding the newline character */
static int io_readline(int fd, io_line_cb *cb, size_t num, void *data)
{
	char buf[BUFSIZ];
	ssize_t len;
	int err;

	len = io_getline(fd, buf, sizeof(buf));
	if (len < 0)
		return len;

	/* replace newline with terminating null byte */
	buf[len - 1] = '\0';

	if (cb) {
		err = cb(buf, num, data);
		if (err)
			return err;
	}

	return 0;
}

/* Read up to count lines excluding their newline character */
int io_readlines(int fd, size_t count, io_line_cb *cb, void *data)
{
	size_t lines = 0;
	int err;

	while (count--) {
		err = io_readline(fd, cb, lines++, data);
		if (err) {
			if (err == -EAGAIN)
				break;

			/* support end-of-file as well */
			if (err == -EPIPE)
				break;

			return err;
		}
	}

	return 0;
}
