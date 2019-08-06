/*
 * map.h - definition of an associative array
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

#ifndef MAP_H
#define MAP_H

struct map;

struct map *map_create(void);
void map_destroy(struct map *map);

int map_copy(struct map *map, const struct map *base);

void map_clear(struct map *map);

int map_set(struct map *map, const char *key, const char *value);
const char *map_get(const struct map *map, const char *key);

typedef int map_func_t(const char *key, const char *value, void *data);
int map_for_each(const struct map *map, map_func_t *func, void *data);

#endif /* MAP_H */
