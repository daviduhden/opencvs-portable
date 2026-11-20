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
/* Portable OpenCVS compatibility functions. SPDX-License-Identifier: ISC */
#include "portable.h"
#include "pc_process.h"
#include <ctype.h>
#undef fclose

char *pc_progname = "opencvs";

const char *
pc_getprogname(void)
{
	return pc_progname;
}

void
pc_platform_startup(int *argc, char ***argv)
{
	char *p;
#ifdef _WIN32
	pc_windows_startup(argc, argv);
#else
	(void)argc;
#endif
	p = strrchr((*argv)[0], '/');
	pc_progname = p ? p + 1 : (*argv)[0];
#ifdef _WIN32
	p = strrchr(pc_progname, '\\');
	if (p != NULL)
		pc_progname = p + 1;
#endif
}

char *
pc_default_tmpdir(void)
{
#ifdef _WIN32
	return pc_windows_tempdir();
#else
	return "/tmp";
#endif
}

#ifndef PC_HAVE_VASPRINTF
int
vasprintf(char **out, const char *fmt, va_list ap)
{
	va_list copy;
	int length, result;
	char *buf;
	*out = NULL;
	va_copy(copy, ap);
	length = vsnprintf(NULL, 0, fmt, copy);
	va_end(copy);
	if (length < 0)
		return -1;
	buf = malloc((size_t)length + 1);
	if (buf == NULL)
		return -1;
	va_copy(copy, ap);
	result = vsnprintf(buf, (size_t)length + 1, fmt, copy);
	va_end(copy);
	if (result != length) {
		free(buf);
		errno = EINVAL;
		return -1;
	}
	*out = buf;
	return length;
}

#endif

#ifndef PC_HAVE_ASPRINTF
int
asprintf(char **out, const char *fmt, ...)
{
	va_list ap;
	int result;
	va_start(ap, fmt);
	result = vasprintf(out, fmt, ap);
	va_end(ap);
	return result;
}
#endif

/* Each stream owns its buffer until the next fgetln or its fclose. */
struct line_buffer {
	FILE *stream;
	char *data;
	size_t capacity;
	struct line_buffer *next;
};
static struct line_buffer *line_buffers;

char *
pc_fgetln(FILE *stream, size_t *length)
{
	struct line_buffer *line;
	size_t used = 0, capacity;
	int ch;
	char *next;
	for (line = line_buffers; line != NULL; line = line->next)
		if (line->stream == stream)
			break;
	if (line == NULL) {
		line = calloc(1, sizeof(*line));
		if (line == NULL)
			return NULL;
		line->stream = stream;
		line->next = line_buffers;
		line_buffers = line;
	}
	while ((ch = getc(stream)) != EOF) {
		if (used == line->capacity) {
			if (line->capacity > SIZE_MAX / 2) {
				errno = ENOMEM;
				return NULL;
			}
			capacity = line->capacity ? line->capacity * 2 : 256;
			next = realloc(line->data, capacity);
			if (next == NULL)
				return NULL;
			line->data = next;
			line->capacity = capacity;
		}
		line->data[used++] = (char)ch;
		if (ch == '\n')
			break;
	}
	*length = used;
	return used ? line->data : NULL;
}

int
pc_fclose(FILE *stream)
{
	struct line_buffer **link = &line_buffers, *line;
	while ((line = *link) != NULL) {
		if (line->stream == stream) {
			*link = line->next;
			free(line->data);
			free(line);
			break;
		}
		link = &line->next;
	}
	return fclose(stream);
}

void
pc_free_argv(char **argv)
{
	size_t i;
	if (argv == NULL)
		return;
	for (i = 0; argv[i] != NULL; i++)
		free(argv[i]);
	free(argv);
}

char **
pc_command_argv(const char *command, const char *last_argument)
{
	char **argv, **next, *word;
	size_t count = 0, used;
	int quote, c;
	const unsigned char *s = (const unsigned char *)command;
	argv = calloc(1, sizeof(*argv));
	if (argv == NULL)
		return NULL;
	while (*s != '\0') {
		while (isspace(*s))
			s++;
		if (*s == '\0')
			break;
		word = malloc(strlen((const char *)s) + 1);
		if (word == NULL)
			goto fail;
		used = 0;
		quote = 0;
		while ((c = *s) != 0) {
			if (quote == 0 && isspace(c))
				break;
			s++;
			if (c == '\'' || c == '"') {
				if (quote == 0) { quote = c; continue; }
				if (quote == c) { quote = 0; continue; }
			}
			if (c == '\\' && quote != '\'' && (*s == '\\' || *s == '"' || *s == '\'' || isspace(*s)))
				c = *s++;
			/* Shell syntax must be explicitly placed in a trusted script. */
			if (quote == 0 && strchr("|;&<>`$", c) != NULL) {
				free(word); errno = EINVAL; goto fail;
			}
			word[used++] = (char)c;
		}
		if (quote != 0) { free(word); errno = EINVAL; goto fail; }
		word[used] = '\0';
		next = reallocarray(argv, count + 2, sizeof(*argv));
		if (next == NULL) { free(word); goto fail; }
		argv = next;
		argv[count++] = word;
		argv[count] = NULL;
	}
	if (count == 0) { errno = EINVAL; goto fail; }
	if (last_argument != NULL) {
		next = reallocarray(argv, count + 2, sizeof(*argv));
		if (next == NULL)
			goto fail;
		argv = next;
		argv[count] = strdup(last_argument);
		if (argv[count] == NULL)
			goto fail;
		argv[++count] = NULL;
	}
	return argv;
fail:
	pc_free_argv(argv);
	return NULL;
}

int
pc_run(const char *const argv[])
{
	pc_process process;
	int result;
	if (pc_spawn(&process, argv, -1, -1, -1) == -1 ||
	    pc_wait(&process, &result) == -1)
		return -1;
	return result;
}

int
pc_any_absolute(const char *path)
{
	return path != NULL && (path[0] == '/' ||
	    (path[0] == '\\' && path[1] == '\\') ||
	    (isalpha((unsigned char)path[0]) && path[1] == ':' &&
	    (path[2] == '/' || path[2] == '\\')));
}

int
pc_native_absolute(const char *path)
{
#ifdef _WIN32
	return pc_any_absolute(path);
#else
	return path != NULL && path[0] == '/';
#endif
}

void
pc_strip_slash(char *path)
{
	size_t length = strlen(path);
	char *cursor;
	if ((length > 1 && path[1] == ':') || path[0] == '\\') {
		for (cursor = path; *cursor != '\0'; cursor++)
			if (*cursor == '\\')
				*cursor = '/';
	}
#ifdef _WIN32
	for (cursor = path; *cursor != '\0'; cursor++)
		if (*cursor == '\\')
			*cursor = '/';
#endif
	while (length > 1 && path[length - 1] == '/') {
		if (length == 3 && isalpha((unsigned char)path[0]) && path[1] == ':')
			break;
		path[--length] = '\0';
	}
}

/* Only CVS metadata and protocol text lines use these CRLF adapters. */
char *
pc_metadata_getln(FILE *stream, size_t *length)
{
	char *line = pc_fgetln(stream, length);
	if (line != NULL && *length >= 2 && line[*length - 2] == '\r' &&
	    line[*length - 1] == '\n') {
		line[*length - 2] = '\n';
		(*length)--;
	}
	return line;
}

char *
pc_metadata_fgets(char *line, int capacity, FILE *stream)
{
	char *result = fgets(line, capacity, stream);
	size_t length;
	if (result == NULL)
		return NULL;
	length = strlen(line);
	if (length >= 2 && line[length - 2] == '\r' && line[length - 1] == '\n') {
		line[length - 2] = '\n';
		line[length - 1] = '\0';
	}
	return result;
}
