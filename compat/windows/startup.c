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
/* Native Unicode entry initialization. ISC license; see LICENSE. */
#include "windows_compat.h"
#include <shellapi.h>
#include <signal.h>

static char **startup_argv;
static int startup_argc;

static void
free_startup_argv(void)
{
	int i;
	for (i = 0; i < startup_argc; i++) free(startup_argv[i]);
	free(startup_argv);
}

static BOOL WINAPI
console_control(DWORD event)
{
	if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT &&
	    event != CTRL_CLOSE_EVENT && event != CTRL_SHUTDOWN_EVENT)
		return FALSE;
	return raise(event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT ? SIGINT : SIGTERM) == 0;
}

void
pc_windows_startup(int *argc, char ***argv)
{
	wchar_t **wide;
	char **utf8;
	int count, i;
	if (_set_fmode(_O_BINARY) != 0 || _setmode(0, _O_BINARY) == -1 ||
	    _setmode(1, _O_BINARY) == -1 || _setmode(2, _O_BINARY) == -1) {
		fputs("opencvs: cannot enable binary standard streams\n", stderr);
		exit(EXIT_FAILURE);
	}
	wide = CommandLineToArgvW(GetCommandLineW(), &count);
	if (wide == NULL || count <= 0) {
		fputs("opencvs: cannot read Unicode command line\n", stderr);
		exit(EXIT_FAILURE);
	}
	utf8 = calloc((size_t)count + 1, sizeof(*utf8));
	if (utf8 == NULL) { LocalFree(wide); exit(EXIT_FAILURE); }
	startup_argv = utf8; startup_argc = count;
	if (atexit(free_startup_argv) != 0) { LocalFree(wide); exit(EXIT_FAILURE); }
	for (i = 0; i < count; i++) {
		utf8[i] = pc_windows_utf8(wide[i]);
		if (utf8[i] == NULL) {
			fputs("opencvs: invalid Unicode command line\n", stderr);
			exit(EXIT_FAILURE);
		}
	}
	LocalFree(wide);
	if (pc_getenv("TMPDIR") == NULL) {
		DWORD length = GetTempPathW(0, NULL);
		wchar_t *temp = malloc(((size_t)length + 1) * sizeof(*temp));
		char *path;
		if (temp == NULL || length == 0 || GetTempPathW(length + 1, temp) == 0) {
			free(temp); exit(EXIT_FAILURE);
		}
		path = pc_windows_utf8(temp); free(temp);
		if (path == NULL || pc_setenv("TMPDIR", path, 0) == -1) { free(path); exit(EXIT_FAILURE); }
		free(path);
	}
	*argc = count; *argv = utf8;
	if (GetConsoleCP() != 0 && !SetConsoleCtrlHandler(console_control, TRUE)) {
		fputs("opencvs: cannot install console cancellation handler\n", stderr);
		exit(EXIT_FAILURE);
	}
}

char *
pc_windows_tempdir(void)
{
	static char *directory;
	wchar_t *wide;
	DWORD len, got;
	if (directory != NULL) return directory;
	len = GetTempPathW(0, NULL);
	if (len == 0) { pc_windows_errno(GetLastError()); return NULL; }
	wide = malloc(((size_t)len + 1) * sizeof(*wide));
	if (wide == NULL) return NULL;
	got = GetTempPathW(len + 1, wide);
	if (got == 0 || got > len) { pc_windows_errno(GetLastError()); free(wide); return NULL; }
	directory = pc_windows_utf8(wide); free(wide);
	if (directory != NULL) for (char *p = directory; *p; p++) if (*p == '\\') *p = '/';
	return directory;
}
