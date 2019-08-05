/*
 * json.c - flat JSON parsing
 * Copyright (C) 2014-2019  Vivien Didelot
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "line.h"
#include "log.h"
#include "map.h"
#include "sys.h"

/* Return the number of UTF-8 bytes on success, 0 if it is invalid */
static size_t json_parse_codepoint(const char *str, char *buf, size_t size)
{
	uint16_t codepoint = 0;
	char utf8[3];
	size_t len;
	int hex;
	char c;
	int i;

	for (i = 0; i < 4; i++) {
		c = str[i];

		if (!isxdigit(c))
			return 0;

		if (c >= '0' && c <= '9')
			hex = c - '0';
		else if (c >= 'a' && c <= 'f')
			hex = c - 'a' + 10;
		else
			hex = c - 'A' + 10;

		codepoint |= hex << (12 - i * 4);
	}

	/* Support Only a single surrogate at the moment */
	if (codepoint <= 0x7f) {
		len = 1;
		utf8[0] = codepoint;
	} else if (codepoint <= 0x7ff) {
		len = 2;
		utf8[0] = 0xc0 | (codepoint >> 6);
		utf8[1] = 0x80 | (codepoint & 0x3f);
	} else {
		len = 3;
		utf8[0] = (0xe0 | (codepoint >> 12));
		utf8[1] = (0x80 | ((codepoint >> 6) & 0x3f));
		utf8[2] = (0x80 | (codepoint & 0x3f));
	}

	if (buf) {
		if (size < len)
			return 0;

		memcpy(buf, utf8, len);
	}

	return len;
}

/* Return the length of the parsed string, 0 if it is invalid */
static size_t json_parse_string(const char *str, char *buf, size_t size)
{
	const char *end = str;
	size_t len;
	char c;

	if (*end != '"')
		return 0;

	do {
		len = 0;

		switch (*++end) {
		case '\0':
			return 0;
		case '"':
			c = '\0';
			break;
		case '\\':
			switch (*++end) {
			case '"':
				c = '"';
				break;
			case '\\':
				c = '\\';
				break;
			case '/':
				c = '/';
				break;
			case 'b':
				c = '\b';
				break;
			case 'f':
				c = '\f';
				break;
			case 'n':
				c = '\n';
				break;
			case 'r':
				c = '\r';
				break;
			case 't':
				c = '\t';
				break;
			case 'u':
				len = json_parse_codepoint(++end, buf, size);
				if (!len)
					return 0;

				end += 3; /* jump to last hex digit */
				break;
			default:
				return 0;
			}
			break;
		default:
			if (iscntrl(*end))
				return 0;

			c = *end;
			break;
		}

		if (buf) {
			if (!len) {
				if (size < 1)
					return 0;

				*buf = c;
				len = 1;
			}

			buf += len;
			size -= len;
		}
	} while (c);

	return ++end - str;
}

/* Return the length of the parsed non scalar (with open/close delimiter), 0 if it is invalid */
static size_t json_parse_nested_struct(const char *str, char open, char close,
				       char *buf, size_t size)
{
	const char *end = str;
	size_t len;
	int nested;

	if (*str != open)
		return 0;

	nested = 1;
	while (nested) {
		++end;

		/* control character or end-of-line? */
		if (iscntrl(*end) || *end == '\0')
			return 0;

		if (*end == open)
			nested++;
		else if (*end == close)
			nested--;
	}

	len = ++end - str;
	if (!len)
		return 0;

	if (buf) {
		if (size <= len)
			return 0;

		strncpy(buf, str, len);
		buf[len] = '\0';
	}

	return len;
}

/* Return the length of the parsed array, 0 if it is invalid */
static size_t json_parse_nested_array(const char *str, char *buf, size_t size)
{
	return json_parse_nested_struct(str, '[', ']', buf, size);
}

/* Return the length of the parsed object, 0 if it is invalid */
static size_t json_parse_nested_object(const char *str, char *buf, size_t size)
{
	return json_parse_nested_struct(str, '{', '}', buf, size);
}

/* Return the length of the parsed number, 0 if it is invalid */
static size_t json_parse_number(const char *str, char *buf, size_t size)
{
	size_t len;
	char *end;

	strtoul(str, &end, 10);

	len = end - str;
	if (!len)
		return 0;

	if (buf) {
		if (size <= len)
			return 0;

		strncpy(buf, str, len);
		buf[len] = '\0';
	}

	return len;
}

/* Return the length of the parsed literal, 0 if it is invalid */
static size_t json_parse_literal(const char *str, const char *literal,
				 char *buf, size_t size)
{
	const size_t len = strlen(literal);

	if (strncmp(str, literal, len) != 0)
		return 0;

	if (buf) {
		strncpy(buf, literal, size);
		if (buf[size - 1] != '\0')
			return 0;
	}

	return len;
}

/* A value can be a string, number, object, array, true, false, or null */
static size_t json_parse_value(const char *str, char *buf, size_t size)
{
	size_t len;

	len = json_parse_string(str, buf, size);
	if (len)
		return len;

	len = json_parse_number(str, buf, size);
	if (len)
		return len;

	len = json_parse_nested_object(str, buf, size);
	if (len)
		return len;

	len = json_parse_nested_array(str, buf, size);
	if (len)
		return len;

	len = json_parse_literal(str, "true", buf, size);
	if (len)
		return len;

	len = json_parse_literal(str, "false", buf, size);
	if (len)
		return len;

	len = json_parse_literal(str, "null", buf, size);
	if (len)
		return len;

	return 0;
}

/* Return the length of a separator optionally enclosed by whitespaces, 0 otherwise */
static size_t json_parse_sep(const char *str, char sep)
{
	size_t len = 0;

	while (isspace(*str))
		str++, len++;

	if (*str != sep)
		return 0;

	str++;
	len++;

	while (isspace(*str))
		str++, len++;

	return len;
}

/* Parse an inline ["name"][\s+:\s+][value] name-value pair */
static size_t json_parse_pair(const char *str, char *name, size_t namesiz,
			      char *val, size_t valsiz)
{
	size_t pair_len = 0;
	size_t len;

	len = json_parse_string(str, name, namesiz);
	if (!len)
		return 0;

	pair_len += len;
	str += len;

	len = json_parse_sep(str, ':');
	if (!len)
		return 0;

	pair_len += len;
	str += len;

	len = json_parse_value(str, val, valsiz);
	if (!len)
		return 0;

	pair_len += len;

	return pair_len;
}

static int json_line_cb(char *line, size_t num, void *data)
{
	struct map *map = data;
	char name[BUFSIZ];
	char val[BUFSIZ];
	size_t len;
	int err;

	for (;;) {
		/* Only support inline flattened structures at the moment */
		while (*line == '[' || *line == ']' || *line == ',' ||
		       *line == '{' || *line == '}' || isspace(*line))
			line++;

		if (*line == '\0')
			break;

		memset(name, 0, sizeof(name));
		memset(val, 0, sizeof(val));

		len = json_parse_pair(line, name, sizeof(name), val,
				      sizeof(val));
		if (!len)
			return -EINVAL;

		line += len;

		/* valid delimiters after a pair */
		if (*line != ',' && *line != '}' && *line != '\0' &&
		    !isspace(*line))
			return -EINVAL;

		if (*line != '\0')
			line++;

		if (map) {
			err = map_set(map, name, val);
			if (err)
				return err;
		}
	}

	return 0;
}

int json_read(int fd, size_t count, struct map *map)
{
	return line_read(fd, count, json_line_cb, map);
}

bool json_is_string(const char *str)
{
	size_t len;

	len = strlen(str);
	if (!len)
		return false;

	return json_parse_string(str, NULL, 0) == len;
}

bool json_is_valid(const char *str)
{
	size_t len;

	len = strlen(str);
	if (!len)
		return false;

	return json_parse_value(str, NULL, 0) == len;
}

int json_escape(const char *str, char *buf, size_t size)
{
	size_t null = strlen(str) + 1;
	char c = '\0';
	int len;

	do {
		switch (c) {
		case '\0':
			len = snprintf(buf, size, "\"");
			break;
		case '\b':
			len = snprintf(buf, size, "\\b");
			break;
		case '\f':
			len = snprintf(buf, size, "\\f");
			break;
		case '\n':
			len = snprintf(buf, size, "\\n");
			break;
		case '\r':
			len = snprintf(buf, size, "\\r");
			break;
		case '\t':
			len = snprintf(buf, size, "\\t");
			break;
		case '\\':
			len = snprintf(buf, size, "\\\\");
			break;
		case '"':
			len = snprintf(buf, size, "\\\"");
			break;
		default:
			if (iscntrl(c))
				len = snprintf(buf, size, "\\u%04x", c);
			else
				len = snprintf(buf, size, "%c", c);
			break;
		}

		/* Ensure the result was not truncated */
		if (len < 0 || len >= size)
			return -ENOSPC;

		size -= len;
		buf += len;

		c = *str;
		if (c)
			str++;
	} while (null--);

	return 0;
}
