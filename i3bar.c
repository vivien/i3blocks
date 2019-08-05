/*
 * i3bar.c - i3bar (plus i3-gaps and sway) protocol support
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

#include "bar.h"
#include "block.h"
#include "json.h"
#include "line.h"
#include "log.h"
#include "map.h"
#include "term.h"

/* See https://i3wm.org/docs/i3bar-protocol.html for details */

static struct {
	const char * const key;
	bool string;
} i3bar_keys[] = {
	{ "", false }, /* unknown key */

	/* Standard keys */
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

	/* i3-gaps features */
	{ "border_top", false },
	{ "border_bottom", false },
	{ "border_left", false },
	{ "border_right", false },
};

static unsigned int i3bar_indexof(const char *key)
{
	unsigned int i;

	for (i = 0; i < sizeof(i3bar_keys) / sizeof(i3bar_keys[0]); i++)
		if (strcmp(i3bar_keys[i].key, key) == 0)
			return i;

	return 0;
}

static int i3bar_line_cb(char *line, size_t num, void *data)
{
	unsigned int index = num + 1;
	struct map *map = data;
	const char *key;

	if (index >= sizeof(i3bar_keys) / sizeof(i3bar_keys[0])) {
		debug("ignoring excess line %d: %s", num, line);
		return 0;
	}

	key = i3bar_keys[index].key;

	return map_set(map, key, line);
}

int i3bar_read(int fd, size_t count, struct map *map)
{
	return line_read(fd, count, i3bar_line_cb, map);
}

static void i3bar_print_term(const struct bar *bar)
{
	struct block *block = bar->blocks;
	const char *full_text;

	term_restore_cursor();

	while (block) {
		full_text = map_get(block->env, "full_text");
		if (full_text)
			fprintf(stdout, "%s ", full_text);

		block = block->next;
	}

	fflush(stdout);
}

static int i3bar_print_pair(const char *key, const char *value, void *data)
{
	unsigned int index = i3bar_indexof(key);
	bool string = i3bar_keys[index].string;
	unsigned int *pcount = data;
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

	if ((*pcount)++)
		fprintf(stdout, ",");

	fprintf(stdout, "\"%s\":%s", key, value);

	return 0;
}

static int i3bar_print_block(struct block *block, void *data)
{
	const char *full_text = map_get(block->env, "full_text");
	unsigned int *mcount = data;
	unsigned int pcount = 0;
	int err;

	/* "full_text" is the only mandatory key */
	if (!full_text) {
		block_debug(block, "no text to display, skipping");
		return 0;
	}

	if ((*mcount)++)
		fprintf(stdout, ",");

	fprintf(stdout, "{");
	err = map_for_each(block->env, i3bar_print_pair, &pcount);
	fprintf(stdout, "}");

	return err;
}

 int i3bar_print(const struct bar *bar)
{
	struct block *block = bar->blocks;
	unsigned int mcount = 0;
	int err;

	if (bar->term) {
		i3bar_print_term(bar);
		return 0;
	}

	fprintf(stdout, ",[");
	while (block) {
		err = i3bar_print_block(block, &mcount);
		if (err)
			break;

		block = block->next;
	}
	fprintf(stdout, "]\n");
	fflush(stdout);

	return err;
}

int i3bar_printf(struct block *block, int lvl, const char *msg)
{
	const struct bar *bar = block->bar;
	struct map *map = block->env;
	int err;

	if (bar->term || lvl > LOG_ERROR)
		return 0;

	block->tainted = true;

	err = map_set(map, "full_text", msg);
	if (err)
		return err;

	if (lvl <= LOG_ERROR) {
		err = map_set(map, "urgent", "true");
		if (err)
			return err;
	}

	return i3bar_print(bar);
}

int i3bar_start(struct bar *bar)
{
	if (bar->term) {
		term_save_cursor();
		term_restore_cursor();
	} else {
		fprintf(stdout, "{\"version\":1,\"click_events\":true}\n[[]\n");
		fflush(stdout);
	}

	return 0;
}

void i3bar_stop(struct bar *bar)
{
	if (bar->term) {
		term_reset_cursor();
	} else {
		fprintf(stdout, "]\n");
		fflush(stdout);
	}
}

static struct block *i3bar_find(struct bar *bar, const struct map *map)
{
	const char *block_name, *block_instance;
	const char *map_name, *map_instance;
	struct block *block = bar->blocks;

	/* "name" and "instance" are the only identifiers provided by i3bar */
	map_name = map_get(map, "name") ? : "";
	map_instance = map_get(map, "instance") ? : "";

	while (block) {
		block_name = block_get(block, "name") ? : "";
		block_instance = block_get(block, "instance") ? : "";

		if (strcmp(block_name, map_name) == 0 &&
		    strcmp(block_instance, map_instance) == 0)
			return block;

		block = block->next;
	}

	return NULL;
}

int i3bar_click(struct bar *bar)
{
	struct block *block;
	struct map *click;
	int err;

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
		block = i3bar_find(bar, click);
		if (block) {
			if (block->tainted) {
				err = block_reset(block);
				if (err)
					break;

				block->tainted = false;

				err = i3bar_print(bar);
				if (err)
					break;
			} else {
				err = map_copy(block->env, click);
				if (err)
					break;

				err = block_click(block);
				if (err)
					break;
			}
		}

		map_clear(click);
	}

	map_destroy(click);

	return err;
}

int i3bar_setup(struct block *block)
{
	const char *instance = map_get(block->config, "instance");
	const char *name = map_get(block->config, "name");
	char buf[BUFSIZ];
	int err;

	/* A block needs a name to be clickable */
	if (!name) {
		name = "foo";
		err = map_set(block->config, "name", name);
		if (err)
			return err;
	}

	if (instance)
		snprintf(buf, sizeof(buf), "%s:%s", name, instance);
	else
		snprintf(buf, sizeof(buf), "%s", name);

	block->name = strdup(buf);
	if (!block->name)
		return -ENOMEM;

	return 0;
}
