/*
 * sys.h - system calls
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

#ifndef SYS_H
#define SYS_H

#include <libgen.h> /* for dirname(3) */
#include <signal.h>
#include <unistd.h>

int sys_chdir(const char *path);

int sys_gettime(unsigned long *interval);
int sys_setitimer(unsigned long interval);

int sys_waitid(pid_t *pid);
int sys_waitpid(pid_t pid, int *code);
int sys_waitanychild(void);

int sys_setenv(const char *name, const char *value);
const char *sys_getenv(const char *name);

int sys_sigemptyset(sigset_t *set);
int sys_sigfillset(sigset_t *set);
int sys_sigaddset(sigset_t *set, int sig);
int sys_sigunblock(const sigset_t *set);
int sys_sigsetmask(const sigset_t *set);
int sys_sigwaitinfo(sigset_t *set, int *sig, int *fd);

int sys_open(const char *path, int *fd);
int sys_close(int fd);
int sys_read(int fd, void *buf, size_t size, size_t *count);
int sys_dup(int fd1, int fd2);
int sys_cloexec(int fd);
int sys_async(int fd, int sig);

int sys_pipe(int *fds);
int sys_fork(pid_t *pid);
void sys_exit(int status);
int sys_execsh(const char *command);

int sys_isatty(int fd);

#endif /* SYS_H */
