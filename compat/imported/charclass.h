/*
 * Portable OpenCVS formatting changes only; original notices below remain in force.
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
/*
 * Public domain, 2008, Todd C. Miller <millert@openbsd.org>
 *
 * $OpenBSD: charclass.h,v 1.3 2020/10/13 04:42:28 guenther Exp $
 */

/*
 * POSIX character class support for fnmatch() and glob().
 */
static const struct cclass {
	const char *name;
	int	    (*isctype)(int);
} cclasses[] = {{"alnum", isalnum}, {"alpha", isalpha}, {"blank", isblank},
    {"cntrl", iscntrl}, {"digit", isdigit}, {"graph", isgraph},
    {"lower", islower}, {"print", isprint}, {"punct", ispunct},
    {"space", isspace}, {"upper", isupper}, {"xdigit", isxdigit}, {NULL, NULL}};

#define NCCLASSES (sizeof(cclasses) / sizeof(cclasses[0]) - 1)
