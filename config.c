/*
 * config.c - parsing of the configuration file
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

#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "ini.h"
#include "log.h"
#include "map.h"
#include "sys.h"

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

struct config {
	struct map *includes;
	struct map *defaults;
	struct map *instance;
	config_cb_t *cb;
	void *data;
	char workdir[PATH_MAX];
};

static int config_include(struct config *conf, const char *path);

static int config_ini_section_cb(char *section, void *data)
{
	struct config *conf = data;
	int err;

	/* flush previous section */
	if (!map_empty(conf->instance)) {
		if (conf->cb) {
			err = conf->cb(conf->instance, conf->data);
			if (err)
				return err;
		}

		map_clear(conf->instance);
	}

	err = map_copy(conf->instance, conf->defaults);
	if (err)
		return err;

	return map_set(conf->instance, "name", section);
}

static int config_ini_property_cb(char *key, char *value, void *data)
{
	struct config *conf = data;
	char cwd[PATH_MAX];
	struct map *map;
	int err;

	if (strcmp(key, "include") == 0) {
		err = config_include(conf, value);
		if (err == -ENOENT)
			err = -EINVAL;

		return err;
	}

	map = map_empty(conf->instance) ? conf->defaults : conf->instance;

	if (strcmp(key, "command") == 0) {
		err = map_set(map, "workdir", conf->workdir);
		if (err)
			return err;
	}

	return map_set(map, key, value);
}

static int config_include(struct config *conf, const char *path)
{
	const char * const home = sys_getenv("HOME");
	char includepath[PATH_MAX];
	char basepath[PATH_MAX];
	char dirpath[PATH_MAX];
	char cwd[PATH_MAX];
	char *base;
	char *dir;
	int err;
	int fd;

	err = sys_getcwd(cwd, sizeof(cwd));
	if (err)
		return err;

	if (strncmp(path, "~/", 2) == 0 && home)
		snprintf(includepath, sizeof(includepath), "%s/%s", home, path + 2);
	else
		snprintf(includepath, sizeof(includepath), "%s", path);

	strcpy(dirpath, includepath);
	dir = dirname(dirpath);

	strcpy(basepath, includepath);
	base = basename(basepath);

	err = sys_chdir(dir);
	if (err)
		return err;

	err = sys_getcwd(conf->workdir, sizeof(conf->workdir));
	if (err)
		return err;

	snprintf(includepath, sizeof(includepath), "%s/%s", conf->workdir, base);

	if (map_get(conf->includes, includepath)) {
		error("include loop detected for %s", includepath);
		return -ELOOP;
	}

	err = map_set(conf->includes, includepath, includepath);
	if (err)
		return err;

	debug("including %s", includepath);

	err = sys_open(base, &fd);
	if (err)
		return err;

	err = ini_read(fd, -1, config_ini_section_cb, config_ini_property_cb, conf);

	sys_close(fd);

	if (err && err != -EAGAIN)
		return err;

	err = map_set(conf->includes, includepath, NULL);
	if (err)
		return err;

	err = sys_chdir(cwd);
	if (err)
		return err;

	strcpy(conf->workdir, cwd);

	return 0;
}

static int config_parse(const char *path, config_cb_t *cb, void *data)
{
	struct config conf = { 0 };
	int err;

	conf.cb = cb;
	conf.data = data;

	conf.includes = map_create();
	if (!conf.includes)
		return -ENOMEM;

	conf.defaults = map_create();
	if (!conf.defaults) {
		map_destroy(conf.includes);
		return -ENOMEM;
	}

	conf.instance = map_create();
	if (!conf.instance) {
		map_destroy(conf.includes);
		map_destroy(conf.defaults);
		return -ENOMEM;
	}

	err = config_include(&conf, path);
	if (err) {
		map_destroy(conf.includes);
		map_destroy(conf.defaults);
		map_destroy(conf.instance);
		return err;
	}

	/* flush last section */
	if (!map_empty(conf.instance) && conf.cb)
		err = conf.cb(conf.instance, conf.data);

	map_destroy(conf.includes);
	map_destroy(conf.defaults);
	map_destroy(conf.instance);
	return err;
}

int config_load(const char *path, config_cb_t *cb, void *data)
{
	const char * const home = sys_getenv("HOME");
	const char * const xdg_home = sys_getenv("XDG_CONFIG_HOME");
	const char * const xdg_dirs = sys_getenv("XDG_CONFIG_DIRS");
	char buf[PATH_MAX];
	int err;

	/* command line config file? */
	if (path)
		return config_parse(path, cb, data);

	/* user config file? */
	if (home) {
		if (xdg_home)
			snprintf(buf, sizeof(buf), "%s/i3blocks/config", xdg_home);
		else
			snprintf(buf, sizeof(buf), "%s/.config/i3blocks/config", home);
		err = config_parse(buf, cb, data);
		if (err != -ENOENT)
			return err;

		snprintf(buf, sizeof(buf), "%s/.i3blocks.conf", home);
		err = config_parse(buf, cb, data);
		if (err != -ENOENT)
			return err;
	}

	/* system config file? */
	if (xdg_dirs)
		snprintf(buf, sizeof(buf), "%s/i3blocks/config", xdg_dirs);
	else
		snprintf(buf, sizeof(buf), "%s/xdg/i3blocks/config", SYSCONFDIR);
	err = config_parse(buf, cb, data);
	if (err != -ENOENT)
		return err;

	snprintf(buf, sizeof(buf), "%s/i3blocks.conf", SYSCONFDIR);
	return config_parse(buf, cb, data);
}
