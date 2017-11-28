/*
 * map.c - implementation of an associative array
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

#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "map.h"

#define PROPERTIES(_) \
	_(full_text,             1024, PROP_I3BAR | PROP_STRING) \
	_(short_text,            512,  PROP_I3BAR | PROP_STRING) \
	_(color,                 8,    PROP_I3BAR | PROP_STRING) \
	_(background,            8,    PROP_I3BAR | PROP_STRING) \
	_(border,                8,    PROP_I3BAR | PROP_STRING) \
	_(min_width,             1024, PROP_I3BAR | PROP_STRING | PROP_NUMBER) \
	_(align,                 8,    PROP_I3BAR | PROP_STRING) \
	_(name,                  32,   PROP_I3BAR | PROP_STRING) \
	_(instance,              256,  PROP_I3BAR | PROP_STRING) \
	_(urgent,                8,    PROP_I3BAR | PROP_BOOLEAN) \
	_(separator,             8,    PROP_I3BAR | PROP_BOOLEAN) \
	_(separator_block_width, 8,    PROP_I3BAR | PROP_NUMBER) \
	_(markup,                8,    PROP_I3BAR | PROP_STRING) \
	_(command,               1024,              PROP_STRING) \
	_(interval,              8,                 PROP_STRING | PROP_NUMBER) \
	_(signal,                8,                 PROP_NUMBER) \
	_(label,                 32,                PROP_STRING) \
	_(format,                8,                 PROP_STRING | PROP_NUMBER) \

struct map {
#define DEFINE(_name, _size, _flags) char _name[_size];
	PROPERTIES(DEFINE);
#undef DEFINE
};

const char *map_get(const struct map *map, const char *key)
{
#define VALUE(_name, _size, _flags) \
	if (strcmp(key, #_name) == 0) \
		return map->_name; \

	PROPERTIES(VALUE);

#undef VALUE

	return NULL;
}

int map_set(struct map *map, const char *key, const char *value)
{
#define VALUE(_name, _size, _flags) \
	if (strcmp(key, #_name) == 0) { \
		strncpy(map->_name, value, _size); \
		return 0; \
	}

	PROPERTIES(VALUE);

#undef VALUE

	return -EINVAL;
}

int map_for_each(const struct map *map, map_func_t *func, void *data)
{
	int err;

#define FOREACH(_name, _size, _flags) \
	err = func(#_name, map->_name, data); \
	if (err) \
		return err;

	PROPERTIES(FOREACH);

#undef FOREACH

	return 0;
}

void map_clear(struct map *map)
{
	memset(map, 0, sizeof(struct map));
}

int map_copy(struct map *map, const struct map *base)
{
	memcpy(map, base, sizeof(struct map));

	return 0;
}

void map_destroy(struct map *map)
{
	free(map);
}

struct map *map_create(void)
{
	return calloc(1, sizeof(struct map));
}
