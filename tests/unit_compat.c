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
/*
 * Native compatibility contract tests for Portable OpenCVS.
 * Copyright (c) 2026, David Uhden Collado <david@uhden.dev>
 * SPDX-License-Identifier: ISC
 *
 * These tests are compiled and executed only by the manual CI workflows.
 */
#include "portable.h"
#include "pc_process.h"

static int failures;

static void
expect(int condition, const char *description)
{
	if (!condition) {
		fprintf(stderr, "compatibility contract failed: %s\n", description);
		failures++;
	}
}

static void
strings(void)
{
	char dst[8], nonterminated[4] = {'a', 'b', 'c', 'd'};
	const char original[4] = {'a', 'b', 'c', 'd'};

	memset(dst, 'X', sizeof(dst));
	expect(strlcpy(dst, "abcdef", 4) == 6, "strlcpy reports source length");
	expect(strcmp(dst, "abc") == 0 && dst[4] == 'X',
	    "strlcpy truncates, terminates, and preserves bounds");
	dst[0] = 'Q';
	expect(strlcpy(dst, "abc", 0) == 3 && dst[0] == 'Q',
	    "strlcpy size zero does not write");
	memcpy(dst, "ab\0XXXXX", sizeof(dst));
	expect(strlcat(dst, "cdefgh", 5) == 8 && strcmp(dst, "abcd") == 0,
	    "strlcat reports combined length and terminates on truncation");
	expect(strlcat(nonterminated, "X", sizeof(nonterminated)) == 5 &&
	    memcmp(nonterminated, original, sizeof(original)) == 0,
	    "strlcat handles a bounded nonterminated destination");
}

static void
numbers_and_memory(void)
{
	const char *error;
	char text[64];
	unsigned char *memory, *replacement;
	volatile size_t huge = SIZE_MAX;
	long long value;

	error = NULL;
	value = strtonum("17", 0, 20, &error);
	expect(value == 17 && error == NULL, "strtonum valid value");
	(void)snprintf(text, sizeof(text), "%lld", LLONG_MIN);
	value = strtonum(text, LLONG_MIN, LLONG_MAX, &error);
	expect(value == LLONG_MIN && error == NULL, "strtonum minimum");
	(void)snprintf(text, sizeof(text), "%lld", LLONG_MAX);
	value = strtonum(text, LLONG_MIN, LLONG_MAX, &error);
	expect(value == LLONG_MAX && error == NULL, "strtonum maximum");
	(void)strtonum("12x", 0, 20, &error);
	expect(error != NULL && strcmp(error, "invalid") == 0,
	    "strtonum rejects trailing garbage");
	(void)strtonum("0", 1, 2, &error);
	expect(error != NULL && strcmp(error, "too small") == 0,
	    "strtonum rejects values below minimum");
	(void)strtonum("18446744073709551616", LLONG_MIN, LLONG_MAX, &error);
	expect(error != NULL && strcmp(error, "too large") == 0,
	    "strtonum rejects integer overflow");

	memory = malloc(16);
	if (memory == NULL) {
		expect(0, "allocate overflow-test original buffer");
		return;
	}
	memset(memory, 0x5a, 16);
	errno = 0;
	/* volatile makes this a runtime overflow contract rather than a compiler
	 * allocation-size diagnostic at the deliberately enormous call site. */
	replacement = reallocarray(memory, huge, 2);
	expect(replacement == NULL && errno == ENOMEM,
	    "reallocarray rejects multiplication overflow");
	if (replacement == NULL) {
		expect(memory[0] == 0x5a && memory[15] == 0x5a,
		    "failed reallocarray preserves the original allocation");
		free(memory);
	} else
		free(replacement);
}

static void
formatting(void)
{
	char *text = NULL;
	int length;

	length = asprintf(&text, "%s", "");
	expect(length == 0 && text != NULL && text[0] == '\0',
	    "asprintf allocates an empty string");
	free(text);
	text = NULL;
	length = asprintf(&text, "%05000d:%s", 7, "end");
	expect(length == 5004 && text != NULL, "asprintf grows for long output");
	if (text != NULL && length == 5004)
		expect(strlen(text) == 5004 && strcmp(text + 5000, ":end") == 0,
		    "asprintf returns complete terminated output");
	free(text);
}

static void
stream_lines(void)
{
	const unsigned char input[] = {'a', '\0', 'b', '\n', 'c'};
	FILE *first = tmpfile(), *second = tmpfile();
	char *line, *other;
	size_t length, other_length;
	int first_closed, second_closed;

	if (first == NULL || second == NULL) {
		expect(0, "create private temporary line streams");
		if (first != NULL)
			(void)fclose(first);
		if (second != NULL)
			(void)fclose(second);
		return;
	}
	expect(fwrite(input, 1, sizeof(input), first) == sizeof(input),
	    "write binary fgetln fixture");
	expect(fwrite("xyz\n", 1, 4, second) == 4,
	    "write interleaved stream fixture");
	rewind(first);
	rewind(second);
	line = fgetln(first, &length);
	expect(line != NULL && length == 4 && memcmp(line, input, 4) == 0,
	    "fgetln preserves embedded NUL and newline length");
	other = fgetln(second, &other_length);
	expect(other != NULL && other_length == 4 && memcmp(other, "xyz\n", 4) == 0,
	    "fgetln reads a second stream");
	expect(line != NULL && length == 4 && memcmp(line, input, 4) == 0,
	    "reading another stream does not invalidate the first buffer");
	line = fgetln(first, &length);
	expect(line != NULL && length == 1 && line[0] == 'c',
	    "fgetln preserves the final unterminated line");
	line = fgetln(first, &length);
	expect(line == NULL && length == 0 && !ferror(first),
	    "fgetln reports clean EOF without stale length");
	first_closed = fclose(first);
	second_closed = fclose(second);
	expect(first_closed == 0 && second_closed == 0,
	    "fclose releases per-stream line buffers");
}

static void
command_arguments(void)
{
	char **args;
	const char *filename = "file ; $(literal) ' with spaces";

	args = pc_command_argv("\"tool with spaces\" --flag 'two words' C:\\work\\name",
	    filename);
	expect(args != NULL, "parse quoted command arguments");
	if (args != NULL) {
		expect(args[0] != NULL && strcmp(args[0], "tool with spaces") == 0 &&
		    args[1] != NULL && strcmp(args[1], "--flag") == 0 &&
		    args[2] != NULL && strcmp(args[2], "two words") == 0 &&
		    args[3] != NULL && strcmp(args[3], "C:\\work\\name") == 0 &&
		    args[4] != NULL && strcmp(args[4], filename) == 0 &&
		    args[5] == NULL,
		    "argv parser preserves quoted words and appends literal filenames");
		pc_free_argv(args);
	}
	errno = 0;
	args = pc_command_argv("tool ; another-command", NULL);
	expect(args == NULL && errno == EINVAL,
	    "argv parser rejects implicit shell operators");
	pc_free_argv(args);
	args = pc_command_argv("tool 'unterminated", NULL);
	expect(args == NULL, "argv parser rejects unterminated quoting");
	pc_free_argv(args);
}

static int
child_arguments(int argc, char **argv)
{
	const char expected[] = "caf\xc3\xa9-\xe9\x9b\xaa";
	const char message[] = "native process argv ok\n";

	if (argc != 6 || strcmp(argv[2], "space argument") != 0 ||
	    strcmp(argv[3], "embedded\"quote") != 0 ||
	    strcmp(argv[4], "trail\\") != 0 || strcmp(argv[5], expected) != 0)
		return 92;
	if (fwrite(message, 1, sizeof(message) - 1, stdout) != sizeof(message) - 1 ||
	    fflush(stdout) != 0)
		return 93;
	return 23;
}

static void
processes(const char *self)
{
	const char *args[] = {self, "--child", "space argument", "embedded\"quote",
	    "trail\\", "caf\xc3\xa9-\xe9\x9b\xaa", NULL};
	const char *exit_args[] = {self, "--exit", NULL};
	const char *missing[] = {"portable-opencvs-test-missing-executable", NULL};
	const char expected[] = "native process argv ok\n";
	pc_process child;
	int fds[2], status;
	char buffer[64];
	size_t used = 0;
	ssize_t n;

	if (pc_pipe(fds) != 0) {
		expect(0, "create native child output pipe");
		return;
	}
	if (pc_spawn(&child, args, -1, fds[1], -1) != 0) {
		expect(0, "spawn native child with quoted Unicode argv");
		(void)close(fds[0]);
		(void)close(fds[1]);
		return;
	}
	(void)close(fds[1]);
	while (used < sizeof(buffer)) {
		n = read(fds[0], buffer + used, 1);
		if (n == -1 && errno == EINTR)
			continue;
		if (n <= 0)
			break;
		used += (size_t)n;
	}
	(void)close(fds[0]);
	expect(n == 0 && used == sizeof(expected) - 1 &&
	    memcmp(buffer, expected, sizeof(expected) - 1) == 0,
	    "native child standard-output pipe preserves bytes");
	expect(pc_wait(&child, &status) == 0 && status == 23,
	    "native process wait preserves nonzero exit status");
	expect(pc_run(exit_args) == 37, "pc_run returns the actual child exit code");
	expect(pc_spawn(&child, missing, -1, -1, -1) == -1,
	    "missing executable fails before reporting a child success");
}

int
main(int argc, char **argv)
{
	pc_platform_startup(&argc, &argv);
	if (argc > 1 && strcmp(argv[1], "--child") == 0)
		return child_arguments(argc, argv);
	if (argc > 1 && strcmp(argv[1], "--exit") == 0)
		return 37;
	strings();
	numbers_and_memory();
	formatting();
	stream_lines();
	command_arguments();
	processes(argv[0]);
	if (failures != 0)
		return 1;
	puts("native compatibility contracts passed");
	return 0;
}
