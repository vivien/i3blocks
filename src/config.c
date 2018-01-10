/*
 * config.c - parsing of the configuration file
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

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>

#include "config.h"
#include "ini.h"
#include "log.h"
#include "map.h"
#include "sys.h"

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

struct config {
	struct map *globals;
	struct map *current;
	config_cb_t *cb;
	void *data;
};

static int config_finalize(struct config *conf)
{
	int err;

	if (conf->current) {
		if (conf->cb) {
			err = conf->cb(conf->current, conf->data);
			if (err)
				return err;
		}

		conf->current = NULL;
	}

	return 0;
}

static int config_reset(struct config *conf)
{
	conf->current = map_create();
	if (!conf->current)
		return -ENOMEM;

	if (conf->globals)
		return map_copy(conf->current, conf->globals);

	return 0;
}

static int config_set(struct config *conf, const char *key, const char *value)
{
	struct map *map = conf->current;

	if (!map) {
		if (!conf->globals) {
			conf->globals = map_create();
			if (!conf->globals)
				return -ENOMEM;
		}

		map = conf->globals;
	}

	return map_set(map, key, value);
}

static int config_ini_section_cb(char *section, void *data)
{
	int err;

	err = config_finalize(data);
	if (err)
		return err;

	err = config_reset(data);
	if (err)
		return err;

	return config_set(data, "name", section);
}

static int config_ini_property_cb(char *key, char *value, void *data)
{
	return config_set(data, key, value);
}

static int config_read(struct config *conf, int fd)
{
	int err;

	err = ini_read(fd, -1, config_ini_section_cb, config_ini_property_cb,
		       conf);
	if (err)
		return err;

	return config_finalize(conf);
}

static int config_open(struct config *conf, const char *path)
{
	int err;
	int fd;

	debug("try file %s", path);

	err = sys_open(path, &fd);
	if (err)
		return err;

	err = config_read(conf, fd);
	sys_close(fd);

	return err;
}

int config_load(const char *path, config_cb_t *cb, void *data)
{
	const char * const home = sys_getenv("HOME");
	const char * const xdg_home = sys_getenv("XDG_CONFIG_HOME");
	const char * const xdg_dirs = sys_getenv("XDG_CONFIG_DIRS");
	struct config conf = {
		.data = data,
		.cb = cb,
	};
	char buf[PATH_MAX];
	int err;


	/* command line config file? */
	if (path)
		return config_open(&conf, path);

	/* user config file? */
	if (home) {
		if (xdg_home)
			snprintf(buf, sizeof(buf), "%s/i3blocks/config", xdg_home);
		else
			snprintf(buf, sizeof(buf), "%s/.config/i3blocks/config", home);
		err = config_open(&conf, buf);
		if (err != -ENOENT)
			return err;

		snprintf(buf, sizeof(buf), "%s/.i3blocks.conf", home);
		err = config_open(&conf, buf);
		if (err != -ENOENT)
			return err;
	}

	/* system config file? */
	if (xdg_dirs)
		snprintf(buf, sizeof(buf), "%s/i3blocks/config", xdg_dirs);
	else
		snprintf(buf, sizeof(buf), "%s/xdg/i3blocks/config", SYSCONFDIR);
	err = config_open(&conf, buf);
	if (err != -ENOENT)
		return err;

	snprintf(buf, sizeof(buf), "%s/i3blocks.conf", SYSCONFDIR);
	return config_open(&conf, buf);
}
