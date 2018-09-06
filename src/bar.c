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
#include "click.h"
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

static bool i3bar_is_string(const char *key)
{
	int i;

	for (i = 0; i < sizeof(i3bar_keys) / sizeof(i3bar_keys[0]); i++)
		if (strcmp(i3bar_keys[i].key, key) == 0)
			return i3bar_keys[i].string;

	return false;
}

static int i3bar_dump_key(const char *key, const char *value, void *data)
{
	char buf[BUFSIZ];
	bool escape;
	int err;

	if (!*value)
		return 0;

	if (i3bar_is_string(key)) {
		if (json_is_string(value))
			escape = false; /* Expected string already quoted */
		else
			escape = true; /* Enforce the string type */
	} else {
		if (json_is_literal(value) || json_is_number(value) ||
		    json_is_string(value))
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
		const char *full_text = block_get(block, "full_text") ? : "";

		/* full_text is the only mandatory key, skip if empty */
		if (!*full_text) {
			block_debug(block, "no text to display, skipping");
			continue;
		}

		i3bar_dump_block(block);
	}

	fprintf(stdout, "]\n");
	fflush(stdout);
}

static void i3bar_start(struct bar *bar)
{
	fprintf(stdout, "{\"version\":1,\"click_events\":true}\n[[]\n");
	fflush(stdout);
}

static void i3bar_stop(struct bar *bar)
{
	fprintf(stdout, "]\n");
	fflush(stdout);
}

void
bar_poll_timed(struct bar *bar)
{
	for (int i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;

		/* spawn unless it is only meant for click or signal */
		if (block->interval != 0) {
			block_spawn(block);
			block_touch(block);
		}
	}
}

static int bar_poll_click(struct click *click, void *data)
{
	struct bar *bar = data;
	struct block *block;
	const char *instance;
	const char *name;
	int i;

	/* find the corresponding block */
	for (i = 0; i < bar->num; ++i) {
		block = bar->blocks + i;
		name = block_get(block, "name") ? : "";
		instance = block_get(block, "instance") ? : "";

		if (strcmp(name, click->name) == 0 &&
		    strcmp(instance, click->instance) == 0) {
			block_debug(block, "clicked");
			block_click(block, click);
			block_spawn(block);
			break; /* Unlikely to click several blocks */
		}
	}

	return 0;
}

void
bar_poll_clicked(struct bar *bar)
{
	int err;

	err = click_read(bar_poll_click, bar);
	if (err)
		error("failed to read clicks");
}

void
bar_poll_outdated(struct bar *bar)
{
	for (int i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;

		if (block->interval > 0) {
			const unsigned long now = sys_time();
			const unsigned long next_update = block->timestamp + block->interval;

			if (((long) (next_update - now)) <= 0) {
				block_debug(block, "outdated");
				block_spawn(block);
				block_touch(block);
			}
		}
	}
}

void
bar_poll_signaled(struct bar *bar, int sig)
{
	for (int i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;

		if (block->signal == sig) {
			block_debug(block, "signaled");
			block_spawn(block);
			block_touch(block);
		}
	}
}

void
bar_poll_exited(struct bar *bar)
{
	pid_t pid;
	int err;

	for (;;) {
		err = sys_waitid(&pid);
		if (err)
			break;

		/* Find the dead process */
		for (int i = 0; i < bar->num; ++i) {
			struct block *block = bar->blocks + i;

			if (block->pid == pid) {
				block_debug(block, "exited");
				block_reap(block);
				if (block->interval == INTER_REPEAT) {
					if (block->timestamp == sys_time())
						block_error(block, "loop too fast");
					block_spawn(block);
					block_touch(block);
				} else if (block->interval == INTER_PERSIST) {
					block_debug(block, "unexpected exit?");
				}
				break;
			}
		}
	}
}

void
bar_poll_readable(struct bar *bar, const int fd)
{
	for (int i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;

		if (block->out[0] == fd) {
			block_debug(block, "readable");
			block_update(block);
			break;
		}
	}
}

void bar_dump(struct bar *bar)
{
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

	block->defaults = map;

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

	err = sched_init(bar);
	if (err)
		fatal("Failed to initialize scheduler");

	sched_start(bar);
}

void bar_destroy(struct bar *bar)
{
	i3bar_stop(bar);

	/* TODO free blocks */
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
