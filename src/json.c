/*
 * json.c - flat JSON parsing
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
#include <stdlib.h>
#include <string.h>

#include "bar.h"
#include "block.h"
#include "io.h"
#include "json.h"
#include "log.h"
#include "sys.h"

struct json {
	char *name;
	size_t name_len;
	char *value;
	size_t value_len;
	json_pair_cb_t *pair_cb;
	void *data;
};


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

static int print_prop(const char *key, const char *value, void *data)
{
	if (!*value)
		return 0;

	fprintf(stdout, ",\"%s\":", key);

	/* Print JSON values as-is, escape them otherwise */
	if (strcmp(value, "true") == 0 || strcmp(value, "false") == 0 ||
	    strcmp(value, "null") == 0 || is_number(value) || *value == '\"')
		fprintf(stdout, "%s", value);
	else
		escape(value);

	return 0;
}

static void
print_block(struct block *block)
{
	fprintf(stdout, ",{\"\":\"\"");
	block_for_each(block, print_prop, NULL);
	fprintf(stdout, "}");
}

void
json_print_bar(struct bar *bar)
{
	fprintf(stdout, ",[{\"full_text\":\"\"}");

	for (int i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;
		const char *full_text = block_get(block, "full_text") ? : "";

		/* full_text is the only mandatory key, skip if empty */
		if (!*full_text) {
			bdebug(block, "no text to display, skipping");
			continue;
		}

		print_block(block);
	}

	fprintf(stdout, "]\n");
	fflush(stdout);
}

static int json_pair(struct json *json)
{
	if (!json->pair_cb)
		return 0;

	*(json->name + json->name_len) = '\0';
	*(json->value + json->value_len) = '\0';

	return json->pair_cb(json->name, json->value, json->data);
}

/* Return the length of the parsed string, 0 if it is invalid */
static size_t json_parse_string(const char *line)
{
	const char *end = line;
	int hex;

	if (*line != '"')
		return 0;

	while (*++end != '"') {
		/* control character or end-of-line? */
		if (iscntrl(*end) || *end == '\0')
			return 0;

		/* any Unicode character except " or \ or control character? */
		if (*end != '\\')
			continue;

		/* backslash escape */
		switch (*++end) {
		case '"':
		case '\\':
		case '/':
		case 'b':
		case 'f':
		case 'n':
		case 'r':
		case 't':
			break;
		case 'u':
			for (hex = 0; hex < 4; hex++)
				if (!isxdigit(*++end))
					return 0;
			break;
		default:
			return 0;
		}
	}

	return ++end - line;
}

/* Return the length of the parsed number, 0 if it is invalid */
static size_t json_parse_number(const char *line)
{
	char *end;

	strtoul(line, &end, 10);

	return end - line;
}

/* Return the length of the parsed litteral, 0 if it is invalid */
static size_t json_parse_litteral(const char *line, const char *litteral)
{
	const size_t len = strlen(litteral);

	if (strncmp(line, litteral, len) != 0)
		return 0;

	return len;
}

/* A value can be a string, number, object, array, true, false, or null */
static size_t json_parse_value(struct json *json, char *line)
{
	json->value = line;

	json->value_len = json_parse_string(line);
	if (json->value_len)
		return json->value_len;

	json->value_len = json_parse_number(line);
	if (json->value_len)
		return json->value_len;

	json->value_len = json_parse_litteral(line, "true");
	if (json->value_len)
		return json->value_len;

	json->value_len = json_parse_litteral(line, "false");
	if (json->value_len)
		return json->value_len;

	json->value_len = json_parse_litteral(line, "null");
	if (json->value_len)
		return json->value_len;

	/* nested arrays or objects are not supported */
	json->value = NULL;

	return 0;
}

/* Return the length of ':' optionally enclosed by whitespaces, 0 otherwise */
static size_t json_parse_colon(const char *line)
{
	size_t len = 0;

	while (isspace(*line))
		line++, len++;

	if (*line != ':')
		return 0;

	line++;
	len++;

	while (isspace(*line))
		line++, len++;

	return len;
}

/* Parse and store (unquoted) the name string */
static size_t json_parse_name(struct json *json, char *line)
{
	size_t len;

	len = json_parse_string(line);
	if (!len)
		return 0;

	json->name = line + 1;
	json->name_len = len - 2;

	return len;
}

/* Parse an inline ["name"][\s+:\s+][value] name-value pair */
static size_t json_parse_pair(struct json *json, char *line)
{
	size_t pair_len = 0;
	size_t len;

	len = json_parse_name(json, line);
	if (!len)
		return 0;

	pair_len += len;
	line += len;

	len = json_parse_colon(line);
	if (!len)
		return 0;

	pair_len += len;
	line += len;

	len = json_parse_value(json, line);
	if (!len)
		return 0;

	pair_len += len;

	return pair_len;
}

static int json_parse_line(char *line, size_t num, void *data)
{
	size_t len;
	int err;

	for (;;) {
		/* only support a flat object at the moment */
		while (*line == '}' || *line == ',' || *line == '{' ||
		       isspace(*line))
			line++;

		if (*line == '\0')
			break;

		len = json_parse_pair(data, line);
		if (!len)
			return -EINVAL;

		line += len;

		/* valid delimiters after a pair */
		if (*line != ',' && *line != '}' && *line != '\0' &&
		    !isspace(*line))
			return -EINVAL;

		if (*line != '\0')
			line++;

		err = json_pair(data);
		if (err)
			return err;
	}

	return 0;
}

int json_read(int fd, size_t count, json_pair_cb_t *pair_cb, void *data)
{
	struct json json = {
		.pair_cb = pair_cb,
		.data = data,
	};

	return io_readlines(fd, count, json_parse_line, &json);
}
