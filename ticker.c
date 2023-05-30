/*
 * ticker.c - ticker implementation
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

#include <stdio.h>
#include <string.h>
#include <utf8proc.h>

#include "ticker.h"
#include "log.h"
#include "sys.h"

static enum ticker_result ticker_scroll(struct ticker *, char *);
static enum ticker_result ticker_convert_utf8_to_mbs(utf8proc_int32_t *, char *,
		unsigned int);

struct ticker *ticker_create(void)
{
	struct ticker *new_ticker = calloc(1, sizeof(struct ticker));
	int err;

	new_ticker->delimeter = TICKER_DELIMETER_DEFAULT;
	new_ticker->direction = TICKER_DIRECTION_DEFAULT;
	new_ticker->chars_limit = TICKER_CHARS_LIMIT_DEFAULT;
	new_ticker->interval = TICKER_INTERVAL_DEFAULT;

	err = sys_gettime(&(new_ticker->timestamp));
	if (err) {
		error("sys_gettime() error, code: %d", err);
		free(new_ticker);

		return NULL;
	}

	new_ticker->full_text_saved = NULL;
	new_ticker->label_saved = NULL;

	memset(new_ticker->buf, '\0', sizeof(char) * BUFSIZ);
	memset(new_ticker->utf8_buf, '\0', sizeof(utf8proc_int32_t) * UTF8_BUFSIZ);
	new_ticker->utf8_buf_strlen = 0;
	new_ticker->offset = 0;

	return new_ticker;
};

void ticker_destroy(struct ticker *ticker)
{
	debug("ticker_destroy()");
	free(ticker->full_text_saved);
	free(ticker->label_saved);
	free(ticker);

	return;
};

char *ticker_output_get(struct ticker *ticker, const char *full_text)
{
	char *ticker_output = calloc((ticker->chars_limit + 1) * sizeof(utf8proc_int32_t),
			sizeof(char));

	if (strcmp(ticker->buf, full_text) != 0) {
		debug("New string");
		ticker->offset = 0;

		memset(ticker->buf, '\0', BUFSIZ * sizeof(char));
		memset(ticker->utf8_buf, '\0', sizeof(utf8proc_int32_t) * UTF8_BUFSIZ);

		snprintf(ticker->buf, sizeof(ticker->buf), "%s", full_text);
		ticker->utf8_buf_strlen =
			utf8proc_decompose((const utf8proc_uint8_t *)ticker->buf, 0,
					ticker->utf8_buf, UTF8_BUFSIZ, UTF8PROC_NULLTERM);
		if (ticker->utf8_buf_strlen < 0) {
			error("Failed to decompose UTF8 multibyte string into an array of codepoints; error code: %d",
					ticker->utf8_buf_strlen);
			memset(ticker->buf, '\0', sizeof(char) * BUFSIZ);
			memset(ticker->utf8_buf, '\0',
					sizeof(utf8proc_int32_t) * UTF8_BUFSIZ);
			free(ticker_output);

			return NULL;
		}
		debug("utf8_buf_strlen: %d", ticker->utf8_buf_strlen);

		*(ticker->utf8_buf + ticker->utf8_buf_strlen) = ' ';
		*(ticker->utf8_buf + ticker->utf8_buf_strlen + 1) = ticker->delimeter;
		*(ticker->utf8_buf + ticker->utf8_buf_strlen + 2) = ' ';
	}

	if (ticker->utf8_buf_strlen <= ticker->chars_limit) {
		debug("utf8_buf_strlen (%d) is less than chars_limit (%d)",
				ticker->utf8_buf_strlen, ticker->chars_limit);
		snprintf(ticker_output, BUFSIZ, "%s", full_text);

		return ticker_output;
	}

	if (ticker_scroll(ticker, ticker_output) != TICKER_RESULT_SUCCESS) {
		error("Failed to scroll string");
		memset(ticker->buf, '\0', sizeof(char) * BUFSIZ);
		memset(ticker->utf8_buf, '\0', sizeof(utf8proc_int32_t) * UTF8_BUFSIZ);
		free(ticker_output);
		return NULL;
	}
	else {
		debug("ticker_output: %s", ticker_output);

		return ticker_output;
	}
};

enum ticker_result ticker_delimeter_set(struct ticker *ticker,
		const char *delimeter)
{
	utf8proc_int32_t utf8_delimeter_str[UTF8_BUFSIZ];
	utf8proc_ssize_t utf8_delimeter_strlen = 0;

	memset(utf8_delimeter_str, '\0', sizeof(utf8proc_int32_t) * UTF8_BUFSIZ);
	utf8_delimeter_strlen =
		utf8proc_decompose((const utf8proc_uint8_t *)delimeter, 0,
				utf8_delimeter_str, UTF8_BUFSIZ, UTF8PROC_NULLTERM);

	if (utf8_delimeter_strlen <= 0) {
		if (utf8_delimeter_strlen != 0)
			error("Failed to decompose UTF8 multibyte string; error code: %d",
					utf8_delimeter_strlen);
		else
			error("Empty delimeter string");

		return TICKER_RESULT_ERR;
	}

	ticker->delimeter = utf8_delimeter_str[0];

	return TICKER_RESULT_SUCCESS;
};

static enum ticker_result ticker_scroll(struct ticker *ticker,
		char *ticker_output)
{
	utf8proc_int32_t utf8_output[UTF8_BUFSIZ];

	memset(utf8_output, '\0', UTF8_BUFSIZ * sizeof(utf8proc_int32_t));
	debug("offset: %d", ticker->offset);

	if (ticker->direction == TICKER_DIRECTION_LEFT)
		if (ticker->offset <= (ticker->utf8_buf_strlen + 3) -
				ticker->chars_limit)
			memcpy(utf8_output, ticker->utf8_buf + ticker->offset,
					ticker->chars_limit * sizeof(utf8proc_int32_t));
		else {
			memcpy(utf8_output,
					ticker->utf8_buf + ticker->offset,
					sizeof(utf8proc_int32_t) * ((ticker->utf8_buf_strlen + 3) -
						ticker->offset));
			memcpy(utf8_output + ((ticker->utf8_buf_strlen + 3) -
						ticker->offset),
					ticker->utf8_buf,
					sizeof(utf8proc_int32_t) * (ticker->chars_limit -
						((ticker->utf8_buf_strlen + 3) - ticker->offset)));
		}
	else
		if (ticker->direction == TICKER_DIRECTION_RIGHT) {
			if (ticker->offset < ticker->chars_limit) {
				memcpy(utf8_output,
						ticker->utf8_buf + (ticker->utf8_buf_strlen + 3) -
						ticker->offset,
						ticker->offset * sizeof(utf8proc_int32_t));
				memcpy(utf8_output + ticker->offset,
						ticker->utf8_buf,
						(ticker->chars_limit - ticker->offset) *
						sizeof(utf8proc_int32_t));
			}
			else
				memcpy(utf8_output,
						ticker->utf8_buf + (ticker->utf8_buf_strlen + 3) -
						ticker->offset,
						ticker->chars_limit * sizeof(utf8proc_int32_t));
		}

	if (ticker->interval > 0) {
		int err;
		unsigned long now;
		const unsigned long next_update = ticker->timestamp + ticker->interval;

		err = sys_gettime(&now);
		if (err) {
			error("sys_gettime() error; code: %d", err);

			return TICKER_RESULT_ERR;
		}

		if (((long) (next_update - now)) <= 0) {
			debug("ticker expired: incrementing offset");

			if (ticker->timestamp == now) {
				debug("looping too fast");

				return TICKER_RESULT_ERR;
			}

			if (ticker->offset >= ticker->utf8_buf_strlen + 2) {
				debug("Resetting offset");
				ticker->offset = 0;
			}
			else
				++(ticker->offset);

			ticker->timestamp = now;
		}
	}

	return ticker_convert_utf8_to_mbs(utf8_output, ticker_output,
			ticker->chars_limit);
};

static enum ticker_result ticker_convert_utf8_to_mbs(utf8proc_int32_t *utf8_str,
		char *mbs, unsigned int n)
{
	utf8proc_uint8_t utf8_codepoint[sizeof(utf8proc_int32_t)];

	for (unsigned int i = 0; i < n; ++i) {
		memset(utf8_codepoint, '\0',
				sizeof(utf8proc_int32_t) * sizeof(utf8proc_uint8_t));

		if (utf8proc_encode_char(*(utf8_str + i), utf8_codepoint))
			strncat(mbs, (const char *)utf8_codepoint,
					sizeof(utf8proc_int32_t));
		else {
			error("Failed to encode char %X", *(utf8_str + i));

			return TICKER_RESULT_ERR;
		}
	}

	return TICKER_RESULT_SUCCESS;
};
