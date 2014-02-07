#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef VERSION
#define VERSION "unknown"
#endif
#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif

struct block {
	/* Keys part of the i3bar protocol */
	char *full_text;
	char *short_text;
	char *color;
	char *min_width;
	char *align;
	char *name;
	char *instance;
	bool urgent;
	bool separator;
	unsigned separator_block_width;

	/* Keys used by i3blocks */
	char *command;
	unsigned interval;
	unsigned timeout;
	unsigned long last_update;
};

struct bar {
	struct block *blocks;
	unsigned int num;
	unsigned int sleeptime;
};

static void
calculate_sleeptime(struct bar *bar)
{
	/* The maximum sleep time is actually the GCD between all block intervals */
	int gcd(int a, int b) {
		while (b != 0)
			a %= b, a ^= b, b ^= a, a ^= b;

		return a;
	}

	bar->sleeptime = 5; /* default */

	if (bar->num >= 2) {
		int i, d;

		d = bar->blocks->interval; /* first block's interval */
		for (i = 0; i < bar->num - 1; ++i)
			d = gcd(d, (bar->blocks + i + 1)->interval);

		if (d > 0)
			bar->sleeptime = d;
	}
}

void
handler(int signum)
{
}

void
init_block(struct block *block)
{
	memset(block, 0, sizeof(struct block));
	block->separator = true;
	block->separator_block_width = 9;
}

struct block *
new_block(struct bar *bar)
{
	struct block *block = NULL;
	void *reloc;

	reloc = realloc(bar->blocks, sizeof(struct block) * (bar->num + 1));
	if (reloc) {
		bar->blocks = reloc;
		block = bar->blocks + bar->num;
		init_block(block);
		bar->num++;
	}

	return block;
}

static void
free_block(struct block *block)
{
	if (block->full_text) free(block->full_text);
	if (block->short_text) free(block->short_text);
	if (block->color) free(block->color);
	if (block->min_width) free(block->min_width);
	if (block->align) free(block->align);
	if (block->name) free(block->name);
	if (block->instance) free(block->instance);
	if (block->command) free(block->command);
	free(block);
}

static char *
parse_section(const char *line)
{
	char *closing = strchr(line, ']');
	int len = strlen(line);

	/* stop if the last char is not a closing bracket */
	if (!closing || line + len - 1 != closing) {
		fprintf(stderr, "malformated section \"%s\"\n", line);
		return NULL;
	}

	return strndup(line + 1, len - 2);
}

static int
parse_property(const char *line, struct block *block)
{
	char *equal = strchr(line, '=');
	char *property;

	if (!equal) {
		fprintf(stderr, "malformated property, should be of the form 'key=value'\n");
		return 1;
	}

	property = equal + 1;

	if (strncmp(line, "command", sizeof("command") - 1) == 0) {
		block->command = strdup(property);
		if (!block->command) return 1;
		return 0;
	}

	if (strncmp(line, "interval", sizeof("interval") - 1) == 0) {
		block->interval = atoi(property); /* FIXME not rigorous */
		return 0;
	}

	if (strncmp(line, "full_text", sizeof("full_text") - 1) == 0) {
		block->full_text = strdup(property);
		if (!block->full_text) return 1;
		return 0;
	}

	if (strncmp(line, "full_text", sizeof("full_text") - 1) == 0) {
		block->full_text = strdup(property);
		if (!block->full_text) return 1;
		return 0;
	}

	if (strncmp(line, "short_text", sizeof("short_text") - 1) == 0) {
		block->short_text = strdup(property);
		if (!block->short_text) return 1;
		return 0;
	}

	if (strncmp(line, "color", sizeof("color") - 1) == 0) {
		block->color = strdup(property);
		if (!block->color) return 1;
		return 0;
	}

	if (strncmp(line, "min_width", sizeof("min_width") - 1) == 0) {
		block->min_width = strdup(property);
		if (!block->min_width) return 1; // FIXME may be a string or a int, should be parsed on json encoding
		return 0;
	}

	if (strncmp(line, "align", sizeof("align") - 1) == 0) {
		block->align = strdup(property);
		if (!block->align) return 1;
		return 0;
	}

	if (strncmp(line, "name", sizeof("name") - 1) == 0) {
		block->name = strdup(property);
		if (!block->name) return 1;
		return 0;
	}

	if (strncmp(line, "instance", sizeof("instance") - 1) == 0) {
		block->instance = strdup(property);
		if (!block->instance) return 1;
		return 0;
	}

	if (strncmp(line, "urgent", sizeof("urgent") - 1) == 0) {
		block->urgent = strcmp(property, "true") == 0; /* FIXME not rigorous */
		return 0;
	}

	if (strncmp(line, "separator", sizeof("separator") - 1) == 0) {
		block->separator = strcmp(property, "true") == 0; /* FIXME not rigorous */
		return 0;
	}

	if (strncmp(line, "separator_block_width", sizeof("separator_block_width") - 1) == 0) {
		block->separator_block_width = atoi(property); /* FIXME not rigorous */
		return 0;
	}

	printf("unknown property: \"%s\"\n", line);
	return 1;
}

static int
parse_config(FILE *fp, struct bar *bar)
{
	struct block *block = bar->blocks;
	char line[1024];
	char *name;

	while (fgets(line, sizeof(line), fp) != NULL) {
		int len = strlen(line);

		if (line[len - 1] != '\n') {
			fprintf(stderr, "line \"%s\" is not terminated by a newline\n", line);
			return 1;
		}
		line[len - 1] = '\0';

		switch (*line) {
		/* Comment or empty line? */
		case '#':
		case '\0':
			break;

		/* Section? */
		case '[':
			name = parse_section(line);
			if (!name)
				return 1;

			block = new_block(bar);
			if (!block) {
				free(name);
				return 1;
			}

			block->name = name;
			/* fprintf(stderr, "new block named: \"%s\"\n", block->name); */
			break;

		/* Property? */
		case 'a' ... 'z':
			if (!block) {
				fprintf(stderr, "global properties not supported, need a section\n");
				return 1;
			}

			if (parse_property(line, block))
				return 1;

			break;

		/* Syntax error */
		default:
			fprintf(stderr, "malformated line \"%s\"\n", line);
			return 1;
		}
	}

	calculate_sleeptime(bar);
	return 0;
}

static void
block_to_json(struct block *block)
{
	fprintf(stdout, "{");

	fprintf(stdout, "\"full_text\":\"%s\"", block->full_text);

	if (block->short_text)
		fprintf(stdout, ",\"short_text\":\"%s\"", block->short_text);
	if (block->color)
		fprintf(stdout, ",\"color\":\"%s\"", block->color);
	if (block->min_width)
		fprintf(stdout, ",\"min_width\":\"%s\"", block->min_width);
	if (block->align)
		fprintf(stdout, ",\"align\":\"%s\"", block->align);
	if (block->name)
		fprintf(stdout, ",\"name\":\"%s\"", block->name);
	if (block->instance)
		fprintf(stdout, ",\"instance\":\"%s\"", block->instance);
	if (block->urgent)
		fprintf(stdout, ",\"urgent\":true");
	if (!block->separator)
		fprintf(stdout, ",\"separator\":false");
	if (block->separator_block_width != 9)
		fprintf(stdout, ",\"separator_block_width\":%d", block->separator_block_width);

	fprintf(stdout, "}");

}

static void
bar_to_json(struct bar *bar)
{
	int i = 0;

	fprintf(stdout, ",[");

	for (i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;

		/* full_text is the only mandatory key */
		if (!block->full_text)
			continue;

		block_to_json(block);
		if (i < bar->num - 1)
			fprintf(stdout, ",");
	}

	fprintf(stdout, "]\n");
	fflush(stdout);
}

static int
update_block(struct block *block)
{
	FILE *fp;
	int status;
	char buf[1024];
	char *line = NULL;

	/* static block */
	if (!block->command)
		return 0;

	/* TODO setup env */
	fp = popen(block->command, "r");
	if (!fp) {
		perror("popen");
		return 1;
	}

	if (fgets(buf, 1024, fp) != NULL) {
		size_t len = strlen(buf);

		/* TODO handle multiline, with short_text and color */
		if (buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
			line = strdup(buf);
		}
	}

	status = pclose(fp);
	switch (status) {
	case -1:
		perror("pclose");
		return 1;
	case 0:
	case 3:
		block->urgent = status == 3;

		if (line) {
			if (block->full_text)
				free(block->full_text);
			block->full_text = line;
		}

		block->last_update = time(NULL);
		break;
	default:
		fprintf(stderr, "bad return code %d, skipping\n", status);
		return 1;
	}

	return 0;
}

static inline int
need_update(struct block *block)
{
	const unsigned long now = time(NULL);
	const unsigned long next_update = block->last_update + block->interval;

	return ((long) (next_update - now)) <= 0;
}

static void
update_bar(struct bar *bar)
{
	int i;

	for (i = 0; i < bar->num; ++i) {
		struct block *block = bar->blocks + i;

		if (need_update(block) && update_block(block))
			fprintf(stderr, "failed to update block %s\n", block->name);
	}
}

static void
start(void)
{
	fprintf(stdout, "{\"version\":1,\"click_events\":true}\n[[]\n");
	fflush(stdout);
}

static void
free_bar(struct bar *bar)
{
	int i;

	for (i = 0; i < bar->num; ++i)
		free_block(bar->blocks + i);

	free(bar);
}

static struct bar *
load_config(const char *inifile)
{
	static const char * const system = SYSCONFDIR "/i3blocks.conf";
	const char * const home = getenv("HOME");
	char buf[PATH_MAX];
	FILE *fp;
	struct bar *bar;

	struct bar *parse(void) {
		bar = calloc(1, sizeof(struct bar));
		if (bar && parse_config(fp, bar)) {
			free_bar(bar);
			bar = NULL;
		}

		if (fclose(fp))
			perror("fclose");

		return bar;
	}

	/* command line config file? */
	if (inifile) {
		fp = fopen(inifile, "r");
		if (!fp) {
			perror("fopen");
			return NULL;
		}

		return parse();
	}

	/* user config file? */
	if (home) {
		snprintf(buf, PATH_MAX, "%s/.i3blocks.conf", home);
		fp = fopen(buf, "r");
		if (fp)
			return parse();

		/* if the file doesn't exist, fall through... */
		if (errno != ENOENT) {
			perror("fopen");
			return NULL;
		}
	}

	/* system config file? */
	fp = fopen(system, "r");
	if (!fp) {
		perror("fopen");
		return NULL;
	}

	return parse();

}

static void
mark_update(struct bar *bar)
{
	int i;

	for (i = 0; i < bar->num; ++i)
		(bar->blocks + i)->last_update = 0;
}

int
main(int argc, char *argv[])
{
	char *inifile = NULL;
	struct sigaction sa;
	struct bar *bar;
	int c;

	while (c = getopt(argc, argv, "c:hv"), c != -1) {
		switch (c) {
		case 'c':
			inifile = optarg;
			break;
		case 'h':
			printf("Usage: %s [-c <configfile>] [-h] [-v]\n", argv[0]);
			return 0;
		case 'v':
			printf("i3blocks " VERSION " © 2014 Vivien Didelot and contributors\n");
			return 0;
		default:
			fprintf(stderr, "Try '%s -h' for more information.\n", argv[0]);
			return 1;
		}
	}

	bar = load_config(inifile);
	if (!bar) {
		fprintf(stderr, "Try '%s -h' for more information.\n", argv[0]);
		return 1;
	}

	sa.sa_handler = handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART; /* Restart functions if interrupted by handler */

	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		fprintf(stderr, "failed to setup a signal handler\n");
		return 1;
	}

	start();

	while (1) {
		update_bar(bar);
		bar_to_json(bar);

		/* Sleep or force check on interruption */
		if (sleep(bar->sleeptime))
			mark_update(bar);
	}

	//stop();
	return 0;
}
