/*
 * map.c - implementation of an associative array
 * Copyright (C) 2017-2019  Vivien Didelot
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
#include <stdlib.h>
#include <string.h>

#include "map.h"

struct pair {
	char *key;
	char *value;

	struct pair *next;
};

struct map {
	struct pair *head;
};

static struct pair *map_head(const struct map *map)
{
	return map->head;
}

/* Return previous pair if key is found, last pair otherwise */
static struct pair *map_prev(const struct map *map, const char *key)
{
	struct pair *prev = map_head(map);
	struct pair *next = prev->next;

	while (next) {
		if (strcmp(next->key, key) == 0)
			break;

		prev = next;
		next = next->next;
	}

	return prev;
}

/* Update the value of a pair */
static int map_reassign(struct pair *pair, const char *value)
{
	free(pair->value);
	pair->value = NULL;

	if (value) {
		pair->value = strdup(value);
		if (!pair->value)
			return -ENOMEM;
	}

	return 0;
}

/* Create a new key-value pair */
static struct pair *map_pair(const char *key, const char *value)
{
	struct pair *pair;
	int err;

	pair = calloc(1, sizeof(struct pair));
	if (!pair)
		return NULL;

	pair->key = strdup(key);
	if (!pair->key) {
		free(pair);
		return NULL;
	}

	err = map_reassign(pair, value);
	if (err) {
		free(pair->key);
		free(pair);
		return NULL;
	}

	return pair;
}

/* Destroy a new key-value pair */
void map_unpair(struct pair *pair)
{
	map_reassign(pair, NULL);
	free(pair->key);
	free(pair);
}

/* Insert a key-value pair after a given pair */
static int map_insert(struct pair *prev, const char *key, const char *value)
{
	struct pair *pair;

	pair = map_pair(key, value);
	if (!pair)
		return -ENOMEM;

	pair->next = prev->next;
	prev->next = pair;

	return 0;
}

/* Delete the key-value pair after a given pair */
static void map_delete(struct pair *prev)
{
	struct pair *pair = prev->next;

	prev->next = pair->next;
	map_unpair(pair);
}

const char *map_get(const struct map *map, const char *key)
{
	struct pair *prev = map_prev(map, key);
	struct pair *next = prev->next;

	if (next)
		return next->value;
	else
		return NULL;
}

int map_set(struct map *map, const char *key, const char *value)
{
	struct pair *prev = map_prev(map, key);
	struct pair *next = prev->next;

	if (next)
		return map_reassign(next, value);
	else
		return map_insert(prev, key, value);
}

int map_for_each(const struct map *map, map_func_t *func, void *data)
{
	struct pair *pair = map_head(map);
	int err;

	while (pair->next) {
		pair = pair->next;
		err = func(pair->key, pair->value, data);
		if (err)
			return err;
	}

	return 0;
}

void map_clear(struct map *map)
{
	struct pair *pair = map_head(map);

	while (pair->next)
		map_delete(pair);
}

static int map_dup(const char *key, const char *value, void *data)
{
	struct map *map = data;

	return map_set(map, key, value);
}

int map_copy(struct map *map, const struct map *base)
{
	return map_for_each(base, map_dup, map);
}

void map_destroy(struct map *map)
{
	map_clear(map);
	free(map->head);
	free(map);
}

struct map *map_create(void)
{
	struct map *map;

	map = calloc(1, sizeof(struct map));
	if (!map)
		return NULL;

	map->head = calloc(1, sizeof(struct pair));
	if (!map->head) {
		free(map);
		return NULL;
	}

	return map;
}
