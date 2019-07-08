/*
 * ini.c - generic INI parser
 * Copyright (C) 2017  Vivien Didelot
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

#include <ctype.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcb_xrm.h>
#include <malloc.h>

#include "ini.h"
#include "line.h"
#include "log.h"

struct ini {
	ini_sec_cb_t *sec_cb;
	ini_prop_cb_t *prop_cb;
	void *data;
    xcb_xrm_database_t *database;
};

static int ini_section(struct ini *ini, char *section)
{
	if (!ini->sec_cb)
		return 0;

	return ini->sec_cb(section, ini->data);
}

static int ini_property(struct ini *ini, char *key, char *value)
{
	if (!ini->prop_cb)
		return 0;

	// If the value begins with 'resource:` then query Xresources for the actual value.
    if (strncmp(value, "resource:", strlen("resource:")) == 0) {
        if (!ini->database) {
            error("Unable to access Xresources.");
            return -EINVAL;
        }

        char *resource;
        if (xcb_xrm_resource_get_string(ini->database, value + strlen("resource:"), NULL, &resource) == 0) {
            value = strdup(resource);
            free(resource);
            printf("Read xres %s\n", value);
        } else {
            error("invalid Xresource key: \"%s\"", value);
            return -EINVAL;
        }
    }

	return ini->prop_cb(key, value, ini->data);
}

static int ini_parse_line(char *line, size_t num, void *data)
{
	/* comment or empty line? */
	if (*line == '\0' || *line == '#')
		return 0;

	/* section? */
	if (*line == '[') {
		char *closing, *section;

		closing = strchr(line, ']');
		if (!closing) {
			error("malformated section \"%s\"", line);
			return -EINVAL;
		}

		if (*(closing + 1) != '\0') {
			error("trailing characters \"%s\"", closing);
			return -EINVAL;
		}

		section = line + 1;
		*closing = '\0';

		return ini_section(data, section);
	}

	/* property? */
	if (isalpha(*line) || *line == '_') {
		char *equals, *key, *value;

		equals = strchr(line, '=');
		if (!equals) {
			error("malformated property, should be a key=value pair");
			return -EINVAL;
		}

		*equals = '\0';
		key = line;
		value = equals + 1;

		return ini_property(data, key, value);
	}

	error("invalid INI syntax for line: \"%s\"", line);
	return -EINVAL;
}

int ini_read(int fd, size_t count, ini_sec_cb_t *sec_cb, ini_prop_cb_t *prop_cb,
	     void *data)
{
    int screen;
    xcb_connection_t *conn = xcb_connect(NULL, &screen);

    xcb_xrm_database_t *xdatabase = xcb_xrm_database_from_default(conn);

	struct ini ini = {
		.sec_cb = sec_cb,
		.prop_cb = prop_cb,
		.data = data,
		.database = xdatabase
	};

	int val = line_read(fd, count, ini_parse_line, &ini);

    xcb_xrm_database_free(xdatabase);

    xcb_disconnect(conn);

	return val;
}
