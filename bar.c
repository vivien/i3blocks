/*
 * bar.c - status line handling functions
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

#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "block.h"
#include "click.h"
#include "json.h"
#include "log.h"

static int
readline(char *buffer, const size_t size)
{
	int nr = 0;
	char c;

	while (nr < size && read(STDIN_FILENO, &c, 1) > 0 && c != '\n')
		buffer[nr++] = c;

	return nr;
}

void
bar_poll_timed(struct bar *bar)
{
	for (int i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;

		/* spawn unless it is only meant for click or signal */
		if (block->interval != 0)
			block_spawn(block, NULL);
	}
}

void
bar_poll_clicked(struct bar *bar)
{
	char json[1024] = { 0 };

	while (readline(json, sizeof(json)) > 0) {
		struct click click;

		/* find the corresponding block */
		click_parse(json, &click);
		if (!*click.name && !*click.instance)
			continue;

		for (int i = 0; i < bar->num; ++i) {
			struct block *block = bar->blocks + i;

			if (strcmp(NAME(block), click.name) == 0 && strcmp(INSTANCE(block), click.instance) == 0) {
				bdebug(block, "clicked");
				block_spawn(block, &click);
				break; /* Unlikely to click several blocks */
			}
		}
	}
}

void
bar_poll_outdated(struct bar *bar)
{
	for (int i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;

		if (block->interval > 0) {
			const unsigned long now = time(NULL);
			const unsigned long next_update = block->timestamp + block->interval;

			if (((long) (next_update - now)) <= 0) {
				bdebug(block, "outdated");
				block_spawn(block, NULL);
			}
		}
	}
}

void
bar_poll_signaled(struct bar *bar, const int sig)
{
	for (int i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;

		if (block->signal == sig) {
			bdebug(block, "signaled");
			block_spawn(block, NULL);
		}
	}
}

void
bar_poll_exited(struct bar *bar)
{
	for (;;) {
		siginfo_t infop = { 0 };

		/* Non-blocking check for dead child(ren) */
		if (waitid(P_ALL, 0, &infop, WEXITED | WNOHANG | WNOWAIT) == -1)
			if (errno != ECHILD)
				errorx("waitid");

		/* Error or no (dead yet) child(ren) */
		if (infop.si_pid == 0)
			break;

		/* Find the dead process */
		for (int i = 0; i < bar->num; ++i) {
			struct block *block = bar->blocks + i;

			if (block->pid == infop.si_pid) {
				bdebug(block, "exited");
				block_reap(block);
				if (block->interval == INTER_REPEAT) {
					if (block->timestamp == time(NULL))
						berror(block, "loop too fast");
					block_spawn(block, NULL);
				}
				break;
			}
		}
	}
}
