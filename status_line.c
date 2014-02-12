/*
 * i3blocks - simple i3 status line
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "status_line.h"

void
calculate_sleeptime(struct status_line *status)
{
	int time = 0;

	/* The maximum sleep time is actually the GCD between all block intervals */
	int gcd(int a, int b) {
		while (b != 0)
			a %= b, a ^= b, b ^= a, a ^= b;

		return a;
	}

	if (status->num > 0) {
		time = status->blocks->interval; /* first block's interval */

		if (status->num >= 2) {
			int i;

			for (i = 1; i < status->num; ++i)
				time = gcd(time, (status->blocks + i)->interval);
		}
	}

	status->sleeptime = time > 0 ? time : 5; /* default */
}

void
update_status_line(struct status_line *status)
{
	int i;

	for (i = 0; i < status->num; ++i) {
		struct block *block = status->blocks + i;

		if (need_update(block) && update_block(block))
			fprintf(stderr, "failed to update block %s\n", block->name);
	}
}

void
free_status_line(struct status_line *status)
{
	int i;

	for (i = 0; i < status->num; ++i)
		free_block(status->blocks + i);

	free(status);
}

void
mark_update(struct status_line *status)
{
	int i;

	for (i = 0; i < status->num; ++i)
		(status->blocks + i)->last_update = 0;
}
