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
#include <sys/stat.h>

#include <fcntl.h>
#include <regex.h>

#include "pc_process.h"
#include "portable.h"

static int failures;

static void
expect(int condition, const char *description)
{
	if (!condition) {
		fprintf(
		    stderr, "compatibility contract failed: %s\n", description);
		failures++;
	}
}

static void
strings(void)
{
	char	   dst[8], nonterminated[4] = {'a', 'b', 'c', 'd'};
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
	const char     *error;
	char		text[64];
	unsigned char  *memory, *replacement;
	volatile size_t huge = SIZE_MAX;
	long long	value;

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
	/* volatile makes this a runtime overflow contract rather than a
	 * compiler allocation-size diagnostic at the deliberately enormous call
	 * site. */
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
	int   length;

	length = asprintf(&text, "%s", "");
	expect(length == 0 && text != NULL && text[0] == '\0',
	    "asprintf allocates an empty string");
	free(text);
	text = NULL;
	length = asprintf(&text, "%05000d:%s", 7, "end");
	expect(
	    length == 5004 && text != NULL, "asprintf grows for long output");
	if (text != NULL && length == 5004)
		expect(strlen(text) == 5004 && strcmp(text + 5000, ":end") == 0,
		    "asprintf returns complete terminated output");
	free(text);
}

static FILE *
temporary_stream(void)
{
#ifdef _MSC_VER
	FILE *stream = NULL;
	if (tmpfile_s(&stream) != 0)
		return NULL;
	return stream;
#else
	return tmpfile();
#endif
}

static void
stream_lines(void)
{
	const unsigned char input[] = {'a', '\0', 'b', '\n', 'c'};
	FILE  *first = temporary_stream(), *second = temporary_stream();
	char  *line, *other;
	size_t length, other_length;
	int    first_closed, second_closed;

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
	expect(other != NULL && other_length == 4 &&
		memcmp(other, "xyz\n", 4) == 0,
	    "fgetln reads a second stream");
	/* A shared scratch buffer would invalidate this pointer here. */
	expect(line != NULL && length == 4 && memcmp(line, input, 4) == 0,
	    "reading another stream does not invalidate the first buffer");
	line = fgetln(first, &length);
	expect(line != NULL && length == 1 && line[0] == 'c',
	    "fgetln preserves the final unterminated line");
	line = fgetln(first, &length);
	expect(line == NULL && feof(first) && !ferror(first),
	    "fgetln reports clean EOF");
	first_closed = fclose(first);
	second_closed = fclose(second);
	expect(first_closed == 0 && second_closed == 0,
	    "fclose releases per-stream line buffers");
}

static void
command_arguments(void)
{
	char	  **args;
	const char *filename = "file ; $(literal) ' with spaces";

	args = pc_command_argv("'/fixture directory/opencvs' server", NULL);
	expect(args != NULL && args[0] != NULL && args[1] != NULL &&
		args[2] == NULL &&
		strcmp(args[0], "/fixture directory/opencvs") == 0 &&
		strcmp(args[1], "server") == 0,
	    "parse GNU CVS single remote command with a quoted executable");
	pc_free_argv(args);

	args = pc_command_argv(
	    "\"tool with spaces\" --flag 'two words' C:\\work\\name", filename);
	expect(args != NULL, "parse quoted command arguments");
	if (args != NULL) {
		expect(args[0] != NULL &&
			strcmp(args[0], "tool with spaces") == 0 &&
			args[1] != NULL && strcmp(args[1], "--flag") == 0 &&
			args[2] != NULL && strcmp(args[2], "two words") == 0 &&
			args[3] != NULL &&
			strcmp(args[3], "C:\\work\\name") == 0 &&
			args[4] != NULL && strcmp(args[4], filename) == 0 &&
			args[5] == NULL,
		    "argv parser preserves quoted words and appends literal "
		    "filenames");
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
	if (fwrite(message, 1, sizeof(message) - 1, stdout) !=
		sizeof(message) - 1 ||
	    fflush(stdout) != 0)
		return 93;
	return 23;
}

static void
processes(const char *self)
{
	const char *args[] = {self, "--child", "space argument",
	    "embedded\"quote", "trail\\", "caf\xc3\xa9-\xe9\x9b\xaa", NULL};
	const char *exit_args[] = {self, "--exit", NULL};
	const char *missing[] = {
	    "portable-opencvs-test-missing-executable", NULL};
	const char expected[] = "native process argv ok\n";
	pc_process child;
	int	   fds[2], status;
	char	   buffer[64];
	size_t	   used = 0;
	ssize_t	   n = -1;

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
	expect(pc_run(exit_args) == 37,
	    "pc_run returns the actual child exit code");
	{
		int launched = pc_spawn(&child, missing, -1, -1, -1);

		/* POSIX can report exec failure as the child's status 127. */
		expect(launched == -1 ? errno == ENOENT :
					launched == 0 &&
			    pc_wait(&child, &status) == 0 && status == 127,
		    "missing executable is reported by spawn or child status");
	}
}

#ifndef _WIN32
static int
copy_child(void)
{
	char	buffer[64];
	ssize_t n, written;
	while ((n = read(0, buffer, sizeof(buffer))) > 0) {
		written = write(1, buffer, (size_t)n);
		if (written != n)
			return 91;
	}
	return n == 0 ? 0 : 91;
}

/* Descriptor mutations are confined to this separate helper process. */
static int
swapped_redirect_child(const char *self)
{
	const char *args[] = {self, "--copy", NULL};
	const char  text[] = "swapped descriptors";
	char	    buffer[sizeof(text)];
	int	    input[2], output[2], status;
	pc_process  child;
	ssize_t	    n;

	if (pc_pipe(input) != 0 || pc_pipe(output) != 0)
		return 91;
	if (write(input[1], text, sizeof(text)) != (ssize_t)sizeof(text))
		return 91;
	(void)close(input[1]);
	if (dup2(input[0], 1) == -1 || dup2(output[1], 0) == -1)
		return 91;
	(void)close(input[0]);
	(void)close(output[1]);
	if (pc_spawn(&child, args, 1, 0, -1) != 0)
		return 91;
	(void)close(0);
	(void)close(1);
	n = read(output[0], buffer, sizeof(buffer));
	(void)close(output[0]);
	if (pc_wait(&child, &status) != 0 || status != 0)
		return 91;
	return n == (ssize_t)sizeof(text) &&
		memcmp(buffer, text, sizeof(text)) == 0 ?
	    0 :
	    91;
}

/*
 * Self-invocation observes descriptor state after exec. A parent-side
 * descriptor check alone would miss an accidental child inheritance.
 */
static void
posix_process_contracts(const char *self)
{
	const char *swapped[] = {self, "--swapped-redirects", NULL};
	const char *both[] = {self, "--both-streams", NULL};
	const char *exit_args[] = {self, "--exit", NULL};
	const char *leak_args[] = {self, "--fd-closed", NULL, NULL};
	pc_process  child, invalid;
	int	    fds[2], status, flags;
	char	    buffer[32], number[32];
	ssize_t	    count;

	expect(pc_run(swapped) == 0,
	    "spawn safely snapshots swapped stdin/stdout sources");
	if (pc_pipe(fds) != 0) {
		expect(0, "create shared output fixture");
		return;
	}
	if (pc_spawn(&child, both, -1, fds[1], fds[1]) != 0) {
		expect(0, "spawn with shared stdout/stderr source");
		(void)close(fds[0]);
		(void)close(fds[1]);
		return;
	}
	(void)close(fds[1]);
	count = read(fds[0], buffer, sizeof(buffer));
	/* Collect both writes; pipe reads may legitimately return only one
	 * write. */
	if (count > 0 && count < 6) {
		ssize_t second = read(
		    fds[0], buffer + count, sizeof(buffer) - (size_t)count);
		if (second > 0)
			count += second;
	}
	(void)close(fds[0]);
	expect(count == 6 && memcmp(buffer, "outERR", 6) == 0,
	    "stdout/stderr may share one redirect source");
	expect(pc_wait(&child, &status) == 0 && status == 0,
	    "wait for shared redirect child");

	if (pc_pipe(fds) != 0) {
		expect(0, "create descriptor leakage fixture");
		return;
	}
	flags = fcntl(fds[0], F_GETFD);
	expect(flags != -1 && (flags & FD_CLOEXEC) != 0,
	    "pipe read end closes on exec");
	flags = fcntl(fds[1], F_GETFD);
	expect(flags != -1 && (flags & FD_CLOEXEC) != 0,
	    "pipe write end closes on exec");
	expect(flags != -1 && fcntl(fds[1], F_SETFD, flags & ~FD_CLOEXEC) == 0,
	    "make caller redirect source explicitly inheritable");
	(void)snprintf(number, sizeof(number), "%d", fds[1]);
	leak_args[2] = number;
	if (pc_spawn(&child, leak_args, -1, fds[1], -1) != 0)
		expect(0, "spawn source-leakage child");
	else
		expect(pc_wait(&child, &status) == 0 && status == 0,
		    "borrowed redirect source is not leaked across exec");
	expect(fcntl(fds[1], F_GETFD) != -1,
	    "parent retains borrowed redirect source");
	(void)close(fds[0]);
	(void)close(fds[1]);

	if (pc_spawn(&child, exit_args, -1, -1, -1) != 0) {
		expect(0, "spawn unrelated wait fixture");
		return;
	}
	invalid.handle = -1;
	errno = 0;
	expect(pc_wait(&invalid, &status) == -1 && errno == EINVAL,
	    "invalid process wait never accepts wildcard child selectors");
	expect(pc_wait(&child, &status) == 0 && status == 37,
	    "invalid wait preserves unrelated child waitability");
}
#endif

static void
positional_reads(void)
{
	char	  *path = NULL, bytes[4];
	const char contents[] = {'A', '\0', 'B', '\n'};
	int	   fd;

	if (asprintf(&path, "%s/pc-pread-XXXXXXXXXX", pc_default_tmpdir()) ==
	    -1) {
		expect(0, "allocate positional-read test path");
		return;
	}
	fd = mkstemp(path);
	if (fd == -1) {
		expect(0, "create positional-read fixture");
		free(path);
		return;
	}
	expect(
	    write(fd, contents, sizeof(contents)) == (ssize_t)sizeof(contents),
	    "write binary positional-read fixture");
	expect(lseek(fd, 2, SEEK_SET) == 2, "set original descriptor position");
	expect(pread(fd, bytes, sizeof(bytes), 0) == (ssize_t)sizeof(bytes) &&
		memcmp(bytes, contents, sizeof(bytes)) == 0,
	    "pread preserves binary bytes");
	expect(lseek(fd, 0, SEEK_CUR) == 2,
	    "pread preserves original descriptor position");
	expect(pread(fd, bytes, sizeof(bytes), 100) == 0, "pread reports EOF");
	errno = 0;
	expect(pread(fd, bytes, sizeof(bytes), -1) == -1 && errno == EINVAL,
	    "pread rejects negative offsets");
	expect(close(fd) == 0, "close positional-read fixture");
	fd = open(path, O_WRONLY);
	if (fd == -1)
		expect(0, "open write-only positional-read fixture");
	else {
		expect(pread(fd, bytes, 1, 0) == -1,
		    "pread rejects write-only descriptors");
		(void)close(fd);
	}
	expect(unlink(path) == 0, "remove positional-read fixture");
	free(path);
}

#ifdef _WIN32
static void
readonly_permissions(void)
{
	char	*path = NULL;
	wchar_t *wide = NULL;
	HANDLE	 deny_delete = INVALID_HANDLE_VALUE;
	DWORD	 attributes;
	int	 fd = -1;

	if (asprintf(&path, "%s/pc-readonly-XXXXXXXXXX", pc_default_tmpdir()) <
		0 ||
	    (fd = mkstemp(path)) == -1 ||
	    (wide = pc_windows_path(path)) == NULL) {
		expect(0, "create native read-only permission fixture");
		goto done;
	}
	if (!SetFileAttributesW(wide, FILE_ATTRIBUTE_READONLY)) {
		expect(0, "set only the read-only file attribute");
		goto done;
	}
	expect(fchmod(fd, 0644) == 0, "descriptor change clears read-only bit");
	attributes = GetFileAttributesW(wide);
	expect(attributes != INVALID_FILE_ATTRIBUTES &&
		(attributes & FILE_ATTRIBUTE_READONLY) == 0,
	    "clearing the last attribute makes the file writable");
	if (!SetFileAttributesW(
		wide, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN)) {
		expect(0, "set hidden and read-only file attributes");
		goto done;
	}
	expect(fchmod(fd, 0644) == 0,
	    "descriptor change preserves unrelated attributes");
	attributes = GetFileAttributesW(wide);
	expect(attributes != INVALID_FILE_ATTRIBUTES &&
		(attributes &
		    (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN)) ==
		    FILE_ATTRIBUTE_HIDDEN,
	    "writable file remains hidden");
	expect(
	    fchmod(fd, 0444) == 0, "set read-only before attempted deletion");
	/* Use a data handle so Windows enforces its delete-sharing mode. */
	deny_delete = CreateFileW(wide, GENERIC_READ,
	    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (deny_delete == INVALID_HANDLE_VALUE) {
		expect(0, "open fixture without sharing delete");
		goto done;
	}
	expect(unlink(path) == -1,
	    "unlink fails while another handle denies deletion");
	attributes = GetFileAttributesW(wide);
	expect(attributes != INVALID_FILE_ATTRIBUTES &&
		(attributes &
		    (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN)) ==
		    (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN),
	    "failed unlink restores read-only and preserves hidden attribute");

done:
	if (deny_delete != INVALID_HANDLE_VALUE)
		CloseHandle(deny_delete);
	if (fd != -1) {
		(void)close(fd);
		expect(unlink(path) == 0,
		    "remove native read-only permission fixture");
	}
	free(wide);
	free(path);
}

static void
readonly_replacement_case(int source_open, int destination_open, int exists)
{
	char	   *from = NULL, *to = NULL;
	char	    byte = '\0';
	struct stat st;
	int source = -1, destination = -1, destination_created = 0, result;

	if (asprintf(&from, "%s/pc-rename-from-XXXXXXXXXX",
		pc_default_tmpdir()) < 0 ||
	    asprintf(&to, "%s/pc-rename-to-XXXXXXXXXX", pc_default_tmpdir()) <
		0) {
		expect(0, "allocate rename fixture names");
		goto done;
	}
	source = mkstemp(from);
	destination = mkstemp(to);
	destination_created = destination != -1;
	if (source == -1 || destination == -1) {
		expect(0, "create rename fixtures");
		goto done;
	}
	if (write(source, "s", 1) != 1 || write(destination, "d", 1) != 1 ||
	    fchmod(source, 0444) == -1 || fchmod(destination, 0444) == -1) {
		expect(0, "prepare read-only rename fixtures");
		goto done;
	}
	if (!source_open) {
		(void)close(source);
		source = -1;
	}
	if (!destination_open || !exists) {
		(void)close(destination);
		destination = -1;
	}
	if (!exists && unlink(to) == -1) {
		expect(0, "prepare absent rename destination");
		goto done;
	}
	result = rename(from, to);
	if (result == -1) {
		/* Preserve diagnostics before stat/open change either error
		 * channel. */
		int	    saved_errno = errno;
		DWORD	    saved_error = GetLastError();
		const char *stage = pc_windows_rename_error_stage();
		fprintf(stderr,
		    "readonly rename source-open=%d destination-open=%d "
		    "exists=%d errno=%d winerror=%lu stage=%s\n",
		    source_open, destination_open, exists, saved_errno,
		    (unsigned long)saved_error, stage);
	}
	expect(result == 0, "replace a read-only file from a read-only source");
	if (destination != -1) {
		(void)close(destination);
		destination = -1;
	}
	if (result == -1) {
		int fd = open(from, O_RDONLY);
		expect(fd != -1 && read(fd, &byte, 1) == 1 && byte == 's',
		    "failed rename preserves source bytes");
		if (fd != -1)
			(void)close(fd);
		expect(stat(from, &st) == 0 && (st.st_mode & S_IWUSR) == 0,
		    "failed rename preserves source read-only attribute");
		if (exists) {
			fd = open(to, O_RDONLY);
			expect(
			    fd != -1 && read(fd, &byte, 1) == 1 && byte == 'd',
			    "failed rename preserves destination bytes");
			if (fd != -1)
				(void)close(fd);
			expect(
			    stat(to, &st) == 0 && (st.st_mode & S_IWUSR) == 0,
			    "failed rename preserves destination read-only "
			    "attribute");
		} else {
			expect(stat(to, &st) == -1 && errno == ENOENT,
			    "failed rename leaves absent destination absent");
		}
	} else {
		expect(stat(to, &st) == 0 && (st.st_mode & S_IWUSR) == 0,
		    "replacement retains its read-only bit");
		destination = open(to, O_RDONLY);
		expect(destination != -1 && read(destination, &byte, 1) == 1 &&
			byte == 's',
		    "replacement retains source bytes");
	}

done:
	if (source != -1)
		(void)close(source);
	if (from != NULL)
		(void)unlink(from);
	if (destination != -1)
		(void)close(destination);
	if (destination_created)
		(void)unlink(to);
	free(from);
	free(to);
}

static void
readonly_replacement(void)
{
	int source_open, destination_open;
	for (source_open = 0; source_open <= 1; source_open++) {
		readonly_replacement_case(source_open, 0, 0);
		for (destination_open = 0; destination_open <= 1;
		    destination_open++)
			readonly_replacement_case(
			    source_open, destination_open, 1);
	}
}
#endif

static void
directory_descriptors(void)
{
	char	   *path = NULL;
	struct stat st;
	int	    fd;

	if (asprintf(&path, "%s/pc-directory-XXXXXXXXXX",
		pc_default_tmpdir()) == -1) {
		expect(0, "allocate private directory test path");
		return;
	}
	fd = mkstemp(path);
	if (fd == -1) {
		expect(0, "reserve unique directory test path");
		free(path);
		return;
	}
	(void)close(fd);
	if (unlink(path) != 0 || mkdir(path, 0700) != 0) {
		expect(0, "create private directory descriptor fixture");
		free(path);
		return;
	}
	fd = open(path, O_RDONLY);
	expect(fd != -1, "open a real native directory descriptor");
	if (fd != -1) {
		expect(fstat(fd, &st) == 0 && S_ISDIR(st.st_mode),
		    "fstat preserves native directory identity");
		expect(close(fd) == 0, "close native directory descriptor");
	}
	expect(rmdir(path) == 0, "remove directory after closing descriptor");
	free(path);
}

static void
regex_contracts(void)
{
	regex_t	   regex;
	regmatch_t match[2];
	int	   result;

	result = regcomp(
	    &regex, "^([[:alpha:]]{2})[0-9]+$", REG_EXTENDED | REG_ICASE);
	expect(result == 0, "compile a quantified expression with a capture");
	if (result != 0)
		return;
	result = regexec(&regex, "Ab123", 2, match, 0);
	expect(result == 0 && match[0].rm_so == 0 && match[0].rm_eo == 5 &&
		match[1].rm_so == 0 && match[1].rm_eo == 2,
	    "match and capture use byte offsets");
	expect(regexec(&regex, "A123", 2, match, 0) == REG_NOMATCH,
	    "reject a nonmatching input");
	regfree(&regex);
}

int
main(int argc, char **argv)
{
	pc_platform_startup(&argc, &argv);
#ifndef _WIN32
	if (argc > 1 && strcmp(argv[1], "--copy") == 0)
		return copy_child();
	if (argc > 1 && strcmp(argv[1], "--swapped-redirects") == 0)
		return swapped_redirect_child(argv[0]);
	if (argc > 1 && strcmp(argv[1], "--both-streams") == 0) {
		return write(1, "out", 3) == 3 && write(2, "ERR", 3) == 3 ? 0 :
									    91;
	}
	if (argc == 3 && strcmp(argv[1], "--fd-closed") == 0) {
		const char *error;
		int	    fd = (int)strtonum(argv[2], 3, INT_MAX, &error);
		if (error != NULL)
			return 91;
		errno = 0;
		return fcntl(fd, F_GETFD) == -1 && errno == EBADF ? 0 : 91;
	}
#endif
	if (argc > 1 && strcmp(argv[1], "--child") == 0)
		return child_arguments(argc, argv);
	if (argc > 1 && strcmp(argv[1], "--exit") == 0)
		return 37;
	strings();
	numbers_and_memory();
	formatting();
	regex_contracts();
	stream_lines();
	directory_descriptors();
#ifdef _WIN32
	readonly_permissions();
	readonly_replacement();
#endif
	positional_reads();
	command_arguments();
	processes(argv[0]);
#ifndef _WIN32
	posix_process_contracts(argv[0]);
#endif
	if (failures != 0)
		return 1;
	puts("native compatibility contracts passed");
	return 0;
}
