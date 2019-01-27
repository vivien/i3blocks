/*
 * bar.c - status line handling functions
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

#include <stdlib.h>
#include <string.h>

#include "bar.h"
#include "block.h"
#include "config.h"
#include "json.h"
#include "log.h"
#include "sched.h"
#include "sys.h"

/* See https://i3wm.org/docs/i3bar-protocol.html for details */

static struct {
	const char * const key;
	bool string;
} i3bar_keys[] = {
	{ "", false }, /* unknown key */

	{ "full_text", true },
	{ "short_text", true },
	{ "color", true },
	{ "background", true },
	{ "border", true },
	{ "min_width", false }, /* can also be a number */
	{ "align", true },
	{ "name", true },
	{ "instance", true },
	{ "urgent", false },
	{ "separator", false },
	{ "separator_block_width", false },
	{ "markup", true },
};

static unsigned int i3bar_indexof(const char *key)
{
	unsigned int i;

	for (i = 0; i < sizeof(i3bar_keys) / sizeof(i3bar_keys[0]); i++)
		if (strcmp(i3bar_keys[i].key, key) == 0)
			return i;

	return 0;
}

static int i3bar_dump_key(const char *key, const char *value, void *data)
{
	unsigned int index = i3bar_indexof(key);
	bool string = i3bar_keys[index].string;
	char buf[BUFSIZ];
	bool escape;
	int err;

	/* Skip unknown keys */
	if (!index)
		return 0;

	if (!value)
		value = "null";

	if (string) {
		if (json_is_string(value))
			escape = false; /* Expected string already quoted */
		else
			escape = true; /* Enforce the string type */
	} else {
		if (json_is_valid(value))
			escape = false; /* Already valid JSON */
		else
			escape = true; /* Unquoted string */
	}

	if (escape) {
		err = json_escape(value, buf, sizeof(buf));
		if (err)
			return err;

		value = buf;
	}

	fprintf(stdout, ",\"%s\":%s", key, value);

	return 0;
}

static void i3bar_dump_block(struct block *block)
{
	fprintf(stdout, ",{\"\":\"\"");
	block_for_each(block, i3bar_dump_key, NULL);
	fprintf(stdout, "}");
}

static void i3bar_dump(struct bar *bar)
{
	int i;

	fprintf(stdout, ",[{\"full_text\":\"\"}");

	for (i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;

		/* full_text is the only mandatory key */
		if (!block_get(block, "full_text")) {
			block_debug(block, "no text to display, skipping");
			continue;
		}

		i3bar_dump_block(block);
	}

	fprintf(stdout, "]\n");
	fflush(stdout);
}

static void bar_freeze(struct bar *bar)
{
	bar->frozen = true;
}

static bool bar_unfreeze(struct bar *bar)
{
	if (bar->frozen) {
		bar->frozen = false;
		return true;
	}

	return false;
}

static bool bar_frozen(struct bar *bar)
{
	return bar->frozen;
}

static void i3bar_log(int lvl, const char *fmt, ...)
{
	const char *color, *urgent, *prefix;
	struct bar *bar = log_data;
	char buf[BUFSIZ];
	va_list ap;

	/* Ignore messages above defined log level and errors */
	if (log_level < lvl || lvl > LOG_ERROR)
		return;

	switch (lvl) {
	case LOG_FATAL:
		prefix = "Fatal! ";
		color = "#FF0000";
		urgent = "true";
		break;
	case LOG_ERROR:
		prefix = "Error: ";
		color = "#FF8000";
		urgent = "true";
		break;
	default:
		prefix = "";
		color = "#FFFFFF";
		urgent = "true";
		break;
	}

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	/* TODO json escape text */
	fprintf(stdout, ",[{");
	fprintf(stdout, "\"full_text\":\"%s%s. Increase log level and/or check stderr for details.\"", prefix, buf);
	fprintf(stdout, ",");
	fprintf(stdout, "\"short_text\":\"%s%s\"", prefix, buf);
	fprintf(stdout, ",");
	fprintf(stdout, "\"urgent\":\"%s\"", urgent);
	fprintf(stdout, ",");
	fprintf(stdout, "\"color\":\"%s\"", color);
	fprintf(stdout, "}]\n");
	fflush(stdout);

	bar_freeze(bar);
}

static void i3bar_start(struct bar *bar)
{
	fprintf(stdout, "{\"version\":1,\"click_events\":true}\n[[]\n");
	fflush(stdout);

	/* From now on the bar can handle log messages */
	log_data = bar;
	log_handle = i3bar_log;
}

static void i3bar_stop(struct bar *bar)
{
	/* From now on the bar can handle log messages */
	log_handle = NULL;
	log_data = NULL;

	fprintf(stdout, "]\n");
	fflush(stdout);
}

static struct block *bar_find(struct bar *bar, const struct map *map)
{
	const char *block_name, *block_instance;
	const char *map_name, *map_instance;
	struct block *block;
	int i;

	/* "name" and "instance" are the only identifiers provided by i3bar */
	map_name = map_get(map, "name") ? : "";
	map_instance = map_get(map, "instance") ? : "";

	for (i = 0; i < bar->num; i++) {
		block = bar->blocks + i;
		block_name = block_get(block, "name") ? : "";
		block_instance = block_get(block, "instance") ? : "";

		if (strcmp(block_name, map_name) == 0 &&
		    strcmp(block_instance, map_instance) == 0)
			return block;
	}

	return NULL;
}

static int bar_click_copy_cb(const char *key, const char *value, void *data)
{
	return block_set(data, key, value);
}

int bar_click(struct bar *bar)
{
	struct block *block;
	struct map *click;
	int err;

	if (bar_unfreeze(bar))
		bar_dump(bar);

	click = map_create();
	if (!click)
		return -ENOMEM;

	for (;;) {
		/* Each click is one JSON object per line */
		err = json_read(STDIN_FILENO, 1, click);
		if (err) {
			if (err == -EAGAIN)
				err = 0;

			break;
		}

		/* Look for the corresponding block */
		block = bar_find(bar, click);
		if (block) {
			err = map_for_each(click, bar_click_copy_cb, block);
			if (err)
				break;

			err = block_click(block);
			if (err)
				break;
		}

		map_clear(click);
	}

	map_destroy(click);

	return err;
}

void bar_dump(struct bar *bar)
{
	if (bar_frozen(bar)) {
		debug("bar frozen, skipping");
		return;
	}

	i3bar_dump(bar);
}

static struct block *bar_add_block(struct bar *bar)
{
	struct block *block = NULL;
	void *reloc;

	reloc = realloc(bar->blocks, sizeof(struct block) * (bar->num + 1));
	if (reloc) {
		bar->blocks = reloc;
		block = bar->blocks + bar->num;
		memset(block, 0, sizeof(*block));
		bar->num++;
	}

	return block;
}

static int bar_config_cb(struct map *map, void *data)
{
	struct bar *bar = data;
	struct block *block;

	block = bar_add_block(bar);
	if (!block)
		return -ENOMEM;

	block->config = map;

	return block_setup(block);
}

void bar_load(struct bar *bar, const char *path)
{
	int err;

	err = config_load(path, bar_config_cb, bar);
	if (err)
		fatal("Failed to load bar configuration file");
}

void bar_schedule(struct bar *bar)
{
	int err;

	/* Initial display (for static blocks and loading labels) */
	bar_dump(bar);

	err = sched_init(bar);
	if (err)
		fatal("Failed to initialize scheduler");

	sched_start(bar);
}

void bar_destroy(struct bar *bar)
{
	int i;

	i3bar_stop(bar);

	for (i = 0; i < bar->num; i++) {
		map_destroy(bar->blocks[i].env);
		map_destroy(bar->blocks[i].config);
	}
	free(bar->blocks);
	free(bar);
}

struct bar *bar_create(void)
{
	struct bar *bar;

	bar = calloc(1, sizeof(struct bar));
	if (!bar)
		return NULL;

	i3bar_start(bar);

	return bar;
}
