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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bar.h"
#include "block.h"

static inline bool
is_number(const char *str)
{
	char *end;

	strtoul(str, &end, 10);

	/* not a valid number if end is not a null character */
	return !(*str == 0 || *end != 0);
}

static inline void
escape(const char *str)
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

static void
print_prop(const char *key, const char *value, int flags)
{
	/* Only print i3bar-specific properties */
	if (!(flags & PROP_I3BAR))
		return;

	if (!*value)
		return;

	fprintf(stdout, ",\"%s\":", key);

	/* Print as-is (except strings which must be escaped) */
	if (flags & PROP_STRING && flags & PROP_NUMBER && is_number(value))
		fprintf(stdout, "%s", value);
	else if (flags & PROP_STRING)
		escape(value);
	else
		fprintf(stdout, "%s", value);
}

static void
print_block(struct block *block)
{
#define PRINT(_name, _size, _flags) \
	print_prop(#_name, block->updated_props._name, _flags); \

	fprintf(stdout, ",{\"\":\"\"");
	PROPERTIES(PRINT);
	fprintf(stdout, "}");

#undef PRINT
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
	const size_t keylen = strlen(name) + 2;
	char key[keylen + 1];
	snprintf(key, sizeof(key), "\"%s\"", name);

	*start = *len = 0;

	char *here = strstr(json, key);
	if (here) {
		here += keylen + 1;

		/* skip pre-value whitespace */
		while (isspace(*here))
			here++;

		if (*here == '"') {
			/* string */
			here++;
			*start = here - json;
			while (*here && *here != '"')
				*len += 1, here++;

			/* invalidate on incomplete string */
			if (*here != '"')
				*start = 0;
		} else {
			/* number */
			*start = here - json;
			while (isdigit(*here++))
				*len += 1;
		}
	}
}

void
json_print_bar(struct bar *bar)
{
	fprintf(stdout, ",[{\"full_text\":\"\"}");

	for (int i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;

		/* full_text is the only mandatory key, skip if empty */
		if (!*FULL_TEXT(block)) {
			bdebug(block, "no text to display, skipping");
			continue;
		}

		print_block(block);
	}

	fprintf(stdout, "]\n");
	fflush(stdout);
}
