/*
 * click.c - read and parse of a click
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

#include <unistd.h>

#include "click.h"
#include "io.h"
#include "json.h"
#include "log.h"

struct click_ctx {
	int (*cb)(struct click *click, void *data);
	void *data;
	struct click click;
};

/*
 * Parse a click, previous read from stdin.
 *
 * A click looks like this ("name" and "instance" can be absent):
 *
 *     ',{"name":"foo","instance":"bar","button":1,"x":1186,"y":13}\n'
 *
 * Note that this function is non-idempotent. We need to parse from right to
 * left. It's ok since the JSON layout is known and fixed.
 */
static int click_parse_line(char *json, size_t num, void *data)
{
	struct click_ctx *ctx = data;
	struct click *click = &ctx->click;
	int nst, nlen;
	int ist, ilen;
	int bst, blen;
	int xst, xlen;
	int yst, ylen;
	int err;

	json_parse(json, "y", &yst, &ylen);
	json_parse(json, "x", &xst, &xlen);
	json_parse(json, "button", &bst, &blen);
	json_parse(json, "instance", &ist, &ilen);
	json_parse(json, "name", &nst, &nlen);

	click->name = json + nst;
	*(click->name + nlen) = '\0';

	click->instance = json + ist;
	*(click->instance + ilen) = '\0';

	click->button = json + bst;
	*(click->button + blen) = '\0';

	click->y = json + yst;
	*(click->y + ylen) = '\0';

	click->x = json + xst;
	*(click->x + xlen) = '\0';

	debug("parsed click: name=%s instance=%s button=%s x=%s y=%s",
			click->name, click->instance,
			click->button, click->x, click->y);

	if (!*click->name && !*click->instance)
		return 0;

	if (ctx->cb) {
		err = ctx->cb(click, ctx->data);
		if (err < 0)
			return err;
	}

	return 0;
}

int click_read(click_cb_t *cb, void *data)
{
	struct click_ctx ctx = {
		.cb = cb,
		.data = data,
	};

	return io_readlines(STDIN_FILENO, -1, click_parse_line, &ctx);
}
