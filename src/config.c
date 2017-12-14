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

#include "bar.h"
#include "block.h"
#include "ini.h"
#include "io.h"
#include "log.h"
#include "map.h"
#include "sys.h"

#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

struct config {
	struct map *defaults;
	struct map *globals;
	struct bar *bar;
};

static struct block *
add_block(struct bar *bar)
{
	struct block *block = NULL;
	void *reloc;

	reloc = realloc(bar->blocks, sizeof(struct block) * (bar->num + 1));
	if (reloc) {
		bar->blocks = reloc;
		block = bar->blocks + bar->num;
		bar->num++;
	}

	return block;
}

static int config_finalize(struct config *conf)
{
	if (conf->defaults) {
		struct block *block;

		block = add_block(conf->bar);
		if (!block)
			return -ENOMEM;

		block->defaults = conf->defaults;

		block_setup(block);

		conf->defaults = NULL;
	}

	return 0;
}

static int config_reset(struct config *conf)
{
	conf->defaults = map_create();
	if (!conf->defaults)
		return -ENOMEM;

	if (conf->globals)
		return map_copy(conf->defaults, conf->globals);

	return 0;
}

static int config_set(struct config *conf, const char *key, const char *value)
{
	struct map *map = conf->defaults;

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

static int config_ini_read(struct config *conf, int fd)
{
	int err;

	err = ini_read(fd, -1, config_ini_section_cb, config_ini_property_cb,
		       conf);
	if (err)
		return err;

	return config_finalize(conf);
}

static struct bar *config_read(int fd)
{
	struct config conf = { 0 };
	int err;

	conf.bar = calloc(1, sizeof(struct bar));
	if (!conf.bar)
		return NULL;

	err = config_ini_read(&conf, fd);
	if (err) {
		free(conf.bar->blocks);
		free(conf.bar);
		return NULL;
	}

	return conf.bar;
}

static struct bar *config_open(const char *path, bool *found)
{
	struct bar *bar = NULL;
	bool noent = false;
	int err;
	int fd;

	debug("try file %s", path);

	err = sys_open(path, &fd);
	if (err) {
		if (err == -ENOENT && found)
			noent = true;
	} else {
		bar = config_read(fd);
		err = sys_close(fd);
		if (err)
			debug("closing \"%s\" failed", path);
	}

	if (found)
		*found = !noent;

	return bar;
}

struct bar *
config_load(const char *inifile)
{
	const char * const home = sys_getenv("HOME");
	const char * const xdg_home = sys_getenv("XDG_CONFIG_HOME");
	const char * const xdg_dirs = sys_getenv("XDG_CONFIG_DIRS");
	char buf[PATH_MAX];
	struct bar *bar;
	bool found;

	/* command line config file? */
	if (inifile)
		return config_open(inifile, NULL);

	/* user config file? */
	if (home) {
		if (xdg_home)
			snprintf(buf, PATH_MAX, "%s/i3blocks/config", xdg_home);
		else
			snprintf(buf, PATH_MAX, "%s/.config/i3blocks/config", home);
		bar = config_open(buf, &found);
		if (found)
			return bar;

		snprintf(buf, PATH_MAX, "%s/.i3blocks.conf", home);
		bar = config_open(buf, &found);
		if (found)
			return bar;
	}

	/* system config file? */
	if (xdg_dirs)
		snprintf(buf, PATH_MAX, "%s/i3blocks/config", xdg_dirs);
	else
		snprintf(buf, PATH_MAX, "%s/xdg/i3blocks/config", SYSCONFDIR);
	bar = config_open(buf, &found);
	if (found)
		return bar;

	snprintf(buf, PATH_MAX, "%s/i3blocks.conf", SYSCONFDIR);
	return config_open(buf, NULL);
}
