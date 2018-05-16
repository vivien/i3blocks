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

#include "click.h"
#include "json.h"
#include "log.h"

/*
 * Parse a click, previous read from stdin.
 *
 * A click looks like this:
 *
 *     ',{"name":"foo","instance":"bar","button":1,"x":1186,"y":13,"relative_x":12,"relative_y":8,"width":50,"height":22}\n'
 *
 * "name" and "instance" can be absent.
 * "relative_x", "relative_y", "width", and "height" introduced in i3 v4.15.
 *
 * Note that this function is non-idempotent. We need to parse from right to
 * left. It's ok since the JSON layout is known and fixed.
 */
void
click_parse(char *json, struct click *click)
{
	int nst, nlen;
	int ist, ilen;
	int bst, blen;
	int xst, xlen;
	int yst, ylen;
	int rxst, rxlen;
	int ryst, rylen;
	int wst, wlen;
	int hst, hlen;

	json_parse(json, "height", &hst, &hlen);
	json_parse(json, "width", &wst, &wlen);
	json_parse(json, "relative_y", &ryst, &rylen);
	json_parse(json, "relative_x", &rxst, &rxlen);
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

	click->x = json + xst;
	*(click->x + xlen) = '\0';

	click->y = json + yst;
	*(click->y + ylen) = '\0';

	click->relative_x = json + rxst;
	*(click->relative_x + rxlen) = '\0';

	click->relative_y = json + ryst;
	*(click->relative_y + rylen) = '\0';

	click->width = json + wst;
	*(click->width + wlen) = '\0';

	click->height = json + hst;
	*(click->height + hlen) = '\0';

	debug("parsed click: name=%s instance=%s button=%s x=%s y=%s relative_x=%s relative_y=%s width=%s height=%s",
			click->name, click->instance,
			click->button, click->x, click->y,
			click->relative_x, click->relative_y,
			click->width, click->height);
}
