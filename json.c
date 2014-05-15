/*
 * json.c - basic JSON parsing and printing
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

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "log.h"

static inline int
is_number(const char *str)
{
	char *end;

	strtoul(str, &end, 10);

	/* not a valid number if end is not a null character */
	return !(*str == 0 || *end != 0);
}

static inline void
print_string(const char *str)
{
	fprintf(stdout, "\"");

	while (*str) {
		switch (*str) {
		case '"':
		case '\\':
			fprintf(stdout, "\\%c", *str);
			break;
		default:
			fprintf(stdout, "%c", *str);
		}

		str++;
	}

	fprintf(stdout, "\"");
}

static inline void
print_number(const char *str)
{
	fprintf(stdout, "%s", str);
}

static inline void
print_boolean(const char *str)
{
	/* FIXME useful? May have been checked during ini parsing */
	fprintf(stdout, strcmp(str, "true") == 0 ? "true" : "false");
}

static inline void
print_string_or_number(const char *str)
{
	is_number(str) ? print_number(str) : print_string(str);
}

static void
block_to_json(struct block *block)
{
	int first = true;

#define JSON(_name, _size, _type) \
	if (*block->_name) { \
		if (!first) fprintf(stdout, ","); \
		else first = false; \
		fprintf(stdout, "\"" #_name "\":"); \
		print_##_type(block->_name); \
	}

	fprintf(stdout, "{");
	PROTOCOL_KEYS(JSON);
	fprintf(stdout, "}");

#undef JSON
}

/*
 * Parse the <json> input for the key <name> and store the start of its value
 * into <start> and its size into <len>.
 *
 * <start> set to 0 means that the key was not present.
 */
void
json_parse(const char *json, const char *name, int *start, int *len)
{
	char *here = strstr(json, name);

	*start = *len = 0;

	if (here) {
		here += strlen(name) + 2;
		if (*here == '"') {
			/* string */
			here++;
			*start = here - json;
			while (*here++ != '"')
				*len += 1;
		} else {
			/* number */
			*start = here - json;
			while (isdigit(*here++))
				*len += 1;
		}
	}
}

void
json_print_status_line(struct status_line *status)
{
	bool first = true;
	int i = 0;

	fprintf(stdout, ",[");

	for (i = 0; i < status->num; ++i) {
		struct block *block = status->updated_blocks + i;

		/* full_text is the only mandatory key, skip if empty */
		if (!*block->full_text) {
			bdebug(block, "no text to display, skipping");
			continue;
		}

		if (!first) fprintf(stdout, ",");
		else first = false;

		bdebug(block, "print json");
		block_to_json(block);
	}

	fprintf(stdout, "]\n");
	fflush(stdout);
}
