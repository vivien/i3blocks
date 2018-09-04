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

#include <string.h>
#include <unistd.h>

#include "click.h"
#include "json.h"
#include "log.h"

static int click_parse(char *name, char *value, void *data)
{
	struct click *click = data;
	char *end;

	if (*value == '"') {
		end = strchr(value, '\0');
		if (!end)
			return -EINVAL; /* unlikely */

		/* unquote strings */
		*--end = '\0';
		value++;
	}

	if (strcmp(name, "name") == 0)
		click->name = value;
	else if (strcmp(name, "instance") == 0)
		click->instance = value;
	else if (strcmp(name, "button") == 0)
		click->button = value;
	else if (strcmp(name, "x") == 0)
		click->x = value;
	else if (strcmp(name, "y") == 0)
		click->y = value;
	else
		debug("unknown key '%s'", name);

	return 0;
}

int click_read(click_cb_t *cb, void *data)
{
	struct click c;
	int err;

	for (;;) {
		memset(&c, 0, sizeof(struct click));
		c.name = "";
		c.instance = "";
		c.button = "";
		c.x = "";
		c.y = "";

		err = json_read(STDIN_FILENO, 1, click_parse, &c);
		if (err) {
			if (err == -EAGAIN)
				break;

			return err;
		}


		debug("read click: name=%s instance=%s button=%s x=%s y=%s",
		      c.name, c.instance, c.button, c.x, c.y);

		if (*c.name == '\0' && *c.instance == '\0')
			break;

		if (cb) {
			err = cb(&c, data);
			if (err)
				return err;
		}
	}

	return 0;
}
