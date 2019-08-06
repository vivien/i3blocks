/*
 * ini.h - generic INI format parsing header
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

#ifndef INI_H
#define INI_H

typedef int ini_sec_cb_t(char *section, void *data);
typedef int ini_prop_cb_t(char *key, char *value, void *data);
int ini_read(int fd, size_t count, ini_sec_cb_t *sec_cb, ini_prop_cb_t *prop_cb,
	     void *data);

#endif /* INI_H */
