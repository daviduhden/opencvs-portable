/*
 * Portable OpenCVS.
 *
 * Copyright (c) 2026, David Uhden Collado <david@uhden.dev>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/* Parser for the ctime-format timestamp stored in CVS/Entries. ISC license. */
#include "windows_compat.h"
#include <ctype.h>

static const char *
number(const char *input, int low, int high, int *out)
{
	int value = 0, digits = 0;
	while (*input == ' ') input++;
	while (*input >= '0' && *input <= '9') {
		if (value > (high - (*input - '0')) / 10) return NULL;
		value = value * 10 + (*input++ - '0'); digits++;
	}
	if (!digits || value < low || value > high) return NULL;
	*out = value; return input;
}

char *
pc_strptime(const char *input, const char *format, struct tm *out)
{
	static const char *const weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char *const months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	const char *p = input;
	int i, year;
	if (input == NULL || format == NULL || out == NULL || strcmp(format, "%a %b %d %T %Y")) { errno = ENOTSUP; return NULL; }
	for (i = 0; i < 7; i++) if (!_strnicmp(p, weekdays[i], 3)) break;
	if (i == 7 || p[3] != ' ') return NULL;
	out->tm_wday = i; p += 4;
	for (i = 0; i < 12; i++) if (!_strnicmp(p, months[i], 3)) break;
	if (i == 12 || p[3] != ' ') return NULL;
	out->tm_mon = i; p += 4;
	p = number(p, 1, 31, &out->tm_mday); if (p == NULL || *p++ != ' ') return NULL;
	p = number(p, 0, 23, &out->tm_hour); if (p == NULL || *p++ != ':') return NULL;
	p = number(p, 0, 59, &out->tm_min); if (p == NULL || *p++ != ':') return NULL;
	p = number(p, 0, 60, &out->tm_sec); if (p == NULL || *p++ != ' ') return NULL;
	p = number(p, 0, 9999, &year); if (p == NULL) return NULL;
	out->tm_year = year - 1900; out->tm_isdst = 0;
	return (char *)p;
}
