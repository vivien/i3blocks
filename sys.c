/*
 * sys.c - system calls
 * Copyright (C) 2017-2019  Vivien Didelot
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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "log.h"

#define sys_errno(msg, ...) \
	trace(msg ": %s", ##__VA_ARGS__, strerror(errno))

int sys_chdir(const char *path)
{
	int rc;

	rc = chdir(path);
	if (rc == -1) {
		sys_errno("chdir(%s)", path);
		rc = -errno;
		return rc;
	}

	return 0;
}

int sys_gettime(unsigned long *interval)
{
	struct timespec ts;
	int rc;

	rc = clock_gettime(CLOCK_MONOTONIC, &ts);
	if (rc == -1) {
		sys_errno("clock_gettime(CLOCK_MONOTONIC)");
		rc = -errno;
		return rc;
	}

	*interval = ts.tv_sec;

	return 0;
}

int sys_setitimer(unsigned long interval)
{
	struct itimerval itv = {
		.it_value.tv_sec = interval,
		.it_interval.tv_sec = interval,
	};
	int rc;

	rc = setitimer(ITIMER_REAL, &itv, NULL);
	if (rc == -1) {
		sys_errno("setitimer(ITIMER_REAL, %ld)", interval);
		rc = -errno;
		return rc;
	}

	return 0;
}

int sys_waitid(pid_t *pid)
{
	siginfo_t infop;
	int rc;

	/* Non-blocking check for dead child(ren) */
	rc = waitid(P_ALL, 0, &infop, WEXITED | WNOHANG | WNOWAIT);
	if (rc == -1) {
		sys_errno("waitid()");
		rc = -errno;
		return rc;
	}

	if (infop.si_pid == 0)
		return -ECHILD;

	*pid = infop.si_pid;

	return 0;
}

int sys_waitpid(pid_t pid, int *code)
{
	int status;
	int rc;

	rc = waitpid(pid, &status, 0);
	if (rc == -1) {
		sys_errno("waitpid(%d)", pid);
		rc = -errno;
		return rc;
	}

	if (rc == 0)
		return -ECHILD;

	if (code)
		*code = WEXITSTATUS(status);

	return 0;
}

int sys_waitanychild(void)
{
	int err;

	for (;;) {
		err = sys_waitpid(-1, NULL);
		if (err) {
			if (err == -ECHILD)
				break;
			return err;
		}
	}

	return 0;
}

int sys_setenv(const char *name, const char *value)
{
	int rc;

	rc = setenv(name, value, 1);
	if (rc == -1) {
		sys_errno("setenv(%s=%s)", name, value);
		rc = -errno;
		return rc;
	}

	return 0;
}

const char *sys_getenv(const char *name)
{
	return getenv(name);
}

int sys_sigemptyset(sigset_t *set)
{
	int rc;

	rc = sigemptyset(set);
	if (rc == -1) {
		sys_errno("sigemptyset()");
		rc = -errno;
		return rc;
	}

	return 0;
}

int sys_sigfillset(sigset_t *set)
{
	int rc;

	rc = sigfillset(set);
	if (rc == -1) {
		sys_errno("sigfillset()");
		rc = -errno;
		return rc;
	}

	return 0;
}

int sys_sigaddset(sigset_t *set, int sig)
{
	int rc;

	rc = sigaddset(set, sig);
	if (rc == -1) {
		sys_errno("sigaddset(%d (%s))", sig, strsignal(sig));
		rc = -errno;
		return rc;
	}

	return 0;
}

static int sys_sigprocmask(const sigset_t *set, int how)
{
	int rc;

	rc = sigprocmask(how, set, NULL);
	if (rc == -1) {
		sys_errno("sigprocmask()");
		rc = -errno;
		return rc;
	}

	return 0;
}

int sys_sigunblock(const sigset_t *set)
{
	return sys_sigprocmask(set, SIG_UNBLOCK);
}

int sys_sigsetmask(const sigset_t *set)
{
	return sys_sigprocmask(set, SIG_SETMASK);
}

int sys_sigwaitinfo(sigset_t *set, int *sig, int *fd)
{
	siginfo_t siginfo;
	int rc;

	rc = sigwaitinfo(set, &siginfo);
	if (rc == -1) {
		sys_errno("sigwaitinfo()");
		rc = -errno;
		return rc;
	}

	*sig = rc;
	*fd = siginfo.si_fd;

	return 0;
}

int sys_open(const char *path, int *fd)
{
	int rc;

	rc = open(path, O_RDONLY | O_NONBLOCK);
	if (rc == -1) {
		sys_errno("open(%s)", path);
		rc = -errno;
		return rc;
	}

	*fd = rc;

	return 0;
}

int sys_close(int fd)
{
	int rc;

	rc = close(fd);
	if (rc == -1) {
		sys_errno("close(%d)", fd);
		rc = -errno;
		return rc;
	}

	return 0;
}

/* Read up to size bytes and store the positive count on success */
int sys_read(int fd, void *buf, size_t size, size_t *count)
{
	ssize_t rc;

	rc = read(fd, buf, size);
	if (rc == -1) {
		sys_errno("read(%d, %ld)", fd, size);
		rc = -errno;
		if (rc == -EWOULDBLOCK)
			rc = -EAGAIN;
		return rc;
	}

	/* End of file or pipe */
	if (rc == 0)
		return -EAGAIN;

	if (count)
		*count = rc;

	return 0;
}

int sys_dup(int fd1, int fd2)
{
	int rc;

	/* Defensive check */
	if (fd1 == fd2)
		return 0;

	/* Close fd2, and reopen bound to fd1 */
	rc = dup2(fd1, fd2);
	if (rc == -1) {
		sys_errno("dup2(%d, %d)", fd1, fd2);
		rc = -errno;
		return rc;
	}

	return 0;
}

static int sys_setsig(int fd, int sig)
{
	int rc;

	rc = fcntl(fd, F_SETSIG, sig);
	if (rc == -1) {
		sys_errno("fcntl(%d, F_SETSIG, %d (%s))", fd, sig,
			  strsignal(sig));
		rc = -errno;
		return rc;
	}

	return 0;
}

static int sys_setown(int fd, pid_t pid)
{
	int rc;

	rc = fcntl(fd, F_SETOWN, pid);
	if (rc == -1) {
		sys_errno("fcntl(%d, F_SETOWN, %d)", fd, pid);
		rc = -errno;
		return rc;
	}

	return 0;
}

static int sys_getfd(int fd, int *flags)
{
	int rc;

	rc = fcntl(fd, F_GETFD);
	if (rc == -1) {
		sys_errno("fcntl(%d, F_GETFD)", fd);
		rc = -errno;
		return rc;
	}

	*flags = rc;

	return 0;
}

static int sys_setfd(int fd, int flags)
{
	int rc;

	rc = fcntl(fd, F_SETFD, flags);
	if (rc == -1) {
		sys_errno("fcntl(%d, F_SETFD, %d)", fd, flags);
		rc = -errno;
		return rc;
	}

	return 0;
}

static int sys_getfl(int fd, int *flags)
{
	int rc;

	rc = fcntl(fd, F_GETFL);
	if (rc == -1) {
		sys_errno("fcntl(%d, F_GETFL)", fd);
		rc = -errno;
		return rc;
	}

	*flags = rc;

	return 0;
}

static int sys_setfl(int fd, int flags)
{
	int rc;

	rc = fcntl(fd, F_SETFL, flags);
	if (rc == -1) {
		sys_errno("fcntl(%d, F_SETFL, %d)", fd, flags);
		rc = -errno;
		return rc;
	}

	return 0;
}

int sys_cloexec(int fd)
{
	int flags;
	int err;

	err = sys_getfd(fd, &flags);
	if (err)
		return err;

	return sys_setfd(fd, flags | FD_CLOEXEC);
}

/* Enable signal-driven I/O, formerly known as asynchronous I/O */
int sys_async(int fd, int sig)
{
	pid_t pid;
	int flags;
	int err;

	err = sys_getfl(fd, &flags);
	if (err)
		return err;

	if (sig) {
    		pid = getpid();
		flags |= (O_ASYNC | O_NONBLOCK);
	} else {
    		pid = 0;
		flags &= ~(O_ASYNC | O_NONBLOCK);
	}

	/* Establish a handler for the signal */
	err = sys_setsig(fd, sig);
	if (err)
		return err;

	/* Set calling process as owner, that is to receive the signal */
	err = sys_setown(fd, pid);
	if (err)
		return err;

	/* Enable/disable nonblocking I/O and signal-driven I/O */
	return sys_setfl(fd, flags);
}

int sys_pipe(int *fds)
{
	int rc;

	rc = pipe(fds);
	if (rc == -1) {
		sys_errno("pipe()");
		rc = -errno;
		return rc;
	}

	return 0;
}

int sys_fork(pid_t *pid)
{
	int rc;

	rc = fork();
	if (rc == -1) {
		sys_errno("fork()");
		rc = -errno;
		return rc;
	}

	*pid = rc;

	return 0;
}

void sys_exit(int status)
{
	_exit(status);
}

int sys_execsh(const char *command)
{
	int rc;

	static const char * const shell = "/bin/sh";

	rc = execl(shell, shell, "-c", command, (char *) NULL);
	if (rc == -1) {
		sys_errno("execl(%s -c \"%s\")", shell, command);
		rc = -errno;
		return rc;
	}

	/* Unreachable */
	return 0;
}

int sys_isatty(int fd)
{
	int rc;

	rc = isatty(fd);
	if (rc == 0) {
		sys_errno("isatty(%d)", fd);
		rc = -errno;
		if (rc == -EINVAL)
			rc = -ENOTTY;
		return rc;
	}

	return 0;
}
