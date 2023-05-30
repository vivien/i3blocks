/*
 * ticker.h - definition of a ticker
 * Copyright (C) 2022-2023 Bogdan Migunov
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

#ifndef TICKER_H
#define TICKER_H

#include <stdio.h>
#include <utf8proc.h>

#define TICKER_CONFIG_OPTION             "ticker"
#define TICKER_CONFIG_OPTION_DELIMETER   "ticker_delimeter"
#define TICKER_CONFIG_OPTION_DIRECTION   "ticker_direction"
#define TICKER_CONFIG_OPTION_CHARS_LIMIT "ticker_chars_limit"
#define TICKER_CONFIG_OPTION_INTERVAL    "ticker_interval"

#define TICKER_DELIMETER_DEFAULT   '|'
#define TICKER_CHARS_LIMIT_DEFAULT 16
#define TICKER_INTERVAL_DEFAULT    1

#define TICKER_DIRECTION_LEFT    0
#define TICKER_DIRECTION_RIGHT   1
#define TICKER_DIRECTION_DEFAULT TICKER_DIRECTION_LEFT

#define UTF8_BUFSIZ ( BUFSIZ / sizeof(utf8proc_int32_t) )

struct ticker {
	utf8proc_int32_t delimeter;
	unsigned char    direction;
	unsigned int     chars_limit;

	unsigned int     interval;
	unsigned long    timestamp;
	char             *full_text_saved;
	char             *label_saved;

	char             buf[BUFSIZ];
	utf8proc_int32_t utf8_buf[UTF8_BUFSIZ];
	utf8proc_ssize_t utf8_buf_strlen;
	unsigned int     offset;
};

enum ticker_result {
	TICKER_RESULT_SUCCESS = 0,
	TICKER_RESULT_ERR,
};

struct ticker *ticker_create(void);
void ticker_destroy(struct ticker *);
char *ticker_output_get(struct ticker *, const char *);
enum ticker_result ticker_delimeter_set(struct ticker *, const char *);

#endif /* TICKER_H */
