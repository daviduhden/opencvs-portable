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
/* Native path splitting with POSIX storage semantics. ISC license. */
#include "windows_compat.h"
#include <libgen.h>
#include <ctype.h>

static size_t
root_length(const char *path)
{
	const char *end;
	if (isalpha((unsigned char)path[0]) && path[1] == ':' && path[2] == '/') return 3;
	if (path[0] == '/' && path[1] == '/' && path[2] != '\0') {
		end = strchr(path + 2, '/');
		if (end != NULL && end[1] != '\0') {
			end = strchr(end + 1, '/');
			return end != NULL ? (size_t)(end - path) : strlen(path);
		}
	}
	return path[0] == '/' ? 1 : 0;
}

static void
forward_slashes(char *path)
{
	for (; *path; path++) if (*path == '\\') *path = '/';
}

char *
pc_basename(char *path)
{
	size_t len, root;
	char *last;
	if (path == NULL || *path == '\0') return ".";
	forward_slashes(path); len = strlen(path); root = root_length(path);
	while (len > root && path[len - 1] == '/') path[--len] = '\0';
	if (len == root && root != 0) return "/";
	last = strrchr(path, '/');
	return last != NULL ? last + 1 : path;
}

char *
pc_dirname(char *path)
{
	size_t len, root;
	char *last;
	if (path == NULL || *path == '\0') return ".";
	forward_slashes(path); len = strlen(path); root = root_length(path);
	while (len > root && path[len - 1] == '/') path[--len] = '\0';
	if (len == root && root != 0) return path;
	last = strrchr(path, '/');
	if (last == NULL) return ".";
	while (last > path + root && last[-1] == '/') last--;
	if ((size_t)(last - path) < root) path[root] = '\0';
	else *last = '\0';
	return path;
}
