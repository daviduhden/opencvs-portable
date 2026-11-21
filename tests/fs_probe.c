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
 * Native filesystem operations for the Perl test harness. Tests communicate
 * UTF-8 paths through an ASCII-named control file so Perl's Windows path
 * encoding does not determine the product's Unicode/long-path result.
 * Copyright (c) 2026, David Uhden Collado <david@uhden.dev>
 * SPDX-License-Identifier: ISC
 */
#include "portable.h"
#include <sys/stat.h>
#include <dirent.h>
#ifndef _WIN32
#include <sys/resource.h>
#include <signal.h>
#endif

static int
separator(char c)
{
#ifdef _WIN32
	return c == '/' || c == '\\';
#else
	return c == '/';
#endif
}

static int
parents(char *path)
{
	size_t i;
	char saved;

	for (i = 1; path[i] != '\0'; i++) {
		if (!separator(path[i]))
			continue;
#ifdef _WIN32
		if (i == 2 && path[1] == ':')
			continue;
		if (i == 1 && separator(path[0]))
			continue;
#endif
		saved = path[i];
		path[i] = '\0';
		if (mkdir(path, 0700) != 0 && errno != EEXIST) {
			path[i] = saved;
			return -1;
		}
		path[i] = saved;
	}
	return 0;
}

static char *
control_path(const char *filename)
{
	FILE *file;
	char *line, *result;
	size_t len;

	file = fopen(filename, "rb");
	if (file == NULL)
		return NULL;
	line = fgetln(file, &len);
	if (line == NULL || len == 0 || len > 65535 ||
	    memchr(line, '\0', len) != NULL) {
		fclose(file);
		errno = EINVAL;
		return NULL;
	}
	if (line[len - 1] == '\n')
		len--;
	if (len != 0 && line[len - 1] == '\r')
		len--;
	if (len == 0) {
		fclose(file);
		errno = EINVAL;
		return NULL;
	}
	result = malloc(len + 1);
	if (result != NULL) {
		memcpy(result, line, len);
		result[len] = '\0';
	}
	if (fclose(file) != 0) {
		free(result);
		return NULL;
	}
	return result;
}

static int
remove_private_tree(const char *path)
{
	DIR *directory;
	struct dirent *entry;
	struct stat st;
	char *child;
	int saved;

	if (chmod(path, 0700) != 0)
		return -1;
	directory = opendir(path);
	if (directory == NULL)
		return -1;
	for (;;) {
		errno = 0;
		entry = readdir(directory);
		if (entry == NULL) {
			if (errno != 0)
				goto fail;
			break;
		}
		if (strcmp(entry->d_name, ".") == 0 ||
		    strcmp(entry->d_name, "..") == 0)
			continue;
		if (asprintf(&child, "%s/%s", path, entry->d_name) == -1)
			goto fail;
		if (lstat(child, &st) != 0) {
			free(child);
			goto fail;
		}
		if (S_ISDIR(st.st_mode)) {
			if (remove_private_tree(child) != 0) {
				free(child);
				goto fail;
			}
		} else {
			/* Never chmod through a link. Native Windows lstat rejects
			 * reparse points; POSIX links are unlinked as links. */
			if ((S_ISREG(st.st_mode) && chmod(child, 0600) != 0) ||
			    unlink(child) != 0) {
				free(child);
				goto fail;
			}
		}
		free(child);
	}
	if (closedir(directory) != 0)
		return -1;
	return rmdir(path);
fail:
	saved = errno;
	(void)closedir(directory);
	errno = saved;
	return -1;
}

static int
cleanup_sandbox(const char *path)
{
	const char *name = path, *scan;
	struct stat st;

	for (scan = path; *scan != '\0'; scan++)
		if (separator(*scan))
			name = scan + 1;
	/* This operation is exclusively for File::Temp-created test roots.
	 * Refuse drive roots, arbitrary paths, and a reparse/link root. */
	if (strncmp(name, "portable-opencvs-", sizeof("portable-opencvs-") - 1) != 0 ||
	    strlen(name) <= sizeof("portable-opencvs-") - 1 ||
	    lstat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
		errno = EINVAL;
		return -1;
	}
	return remove_private_tree(path);
}

/* Deliberately untrusted server fixtures, confined to the test subprocess. */
static int
fixture_server(const char *response)
{
	static const char capabilities[] = "Valid-requests Root Valid-responses valid-requests "
	    "Directory Static-directory Sticky Entry Modified UseUnchanged Unchanged "
	    "Questionable Argument Argumentx Global_option Set update\nok\n";
	char *line, *length_line;
	unsigned char block[4096];
	size_t length, count, remaining;
	FILE *fixture;
	const char *error;
	int negotiated = 0;

	while ((line = fgetln(stdin, &length)) != NULL) {
		if (length == 15 && memcmp(line, "valid-requests\n", 15) == 0) {
			if (fputs(capabilities, stdout) == EOF || fflush(stdout) != 0) return 74;
			negotiated = 1;
		} else if (length >= 9 && memcmp(line, "Modified ", 9) == 0) {
			if (fgetln(stdin, &length) == NULL) return 74; /* mode */
			length_line = fgetln(stdin, &length);
			if (length_line == NULL || length == 0 || length_line[length - 1] != '\n') return 74;
			length_line[length - 1] = '\0';
			remaining = (size_t)strtonum(length_line, 0, INT_MAX, &error);
			if (error != NULL) return 74;
			while (remaining != 0) {
				count = remaining < sizeof(block) ? remaining : sizeof(block);
				count = fread(block, 1, count, stdin);
				if (count == 0) return 74;
				remaining -= count;
			}
		} else if (length == 7 && memcmp(line, "update\n", 7) == 0) {
			if (!negotiated) return 74;
			fixture = fopen(response, "rb");
			if (fixture == NULL) return 74;
			while ((count = fread(block, 1, sizeof(block), fixture)) != 0)
				if (fwrite(block, 1, count, stdout) != count) { fclose(fixture); return 74; }
			if (ferror(fixture)) { fclose(fixture); return 74; }
			if (fclose(fixture) != 0 || fflush(stdout) != 0) return 74;
			return getenv("PC_TEST_SERVER_FAIL_AFTER_RESPONSE") != NULL ? 23 : 0;
		}
	}
	return 74;
}

int
main(int argc, char **argv)
{
	char *target;
	const char *source_name, *destination_name;
	FILE *source = NULL, *destination = NULL;
	unsigned char buffer[8192];
	size_t n;
	int result = 1;

	pc_platform_startup(&argc, &argv);
	if (argc == 2 && strcmp(argv[1], "server") == 0 &&
	    getenv("PC_TEST_SERVER_RESPONSE") != NULL)
		return fixture_server(getenv("PC_TEST_SERVER_RESPONSE"));
	if (argc == 5 && strcmp(argv[1], "verify-message") == 0) {
		FILE *message = fopen(argv[4], "rb"), *record;
		int failed;
		if (message == NULL)
			return 24;
		n = fread(buffer, 1, sizeof(buffer), message);
		failed = ferror(message);
		if (fclose(message) != 0 || failed)
			return 24;
		record = fopen(argv[2], "wb");
		if (record == NULL)
			return 24;
		failed = fwrite(buffer, 1, n, record) != n;
		if (fclose(record) != 0 || failed)
			return 24;
		return n == strlen(argv[3]) && memcmp(buffer, argv[3], n) == 0 ? 0 : 23;
	}
	if (argc == 4 && strcmp(argv[1], "editor-check-template") == 0) {
		static const char expected[] = "Portable OpenCVS template sentinel";
		FILE *message = fopen(argv[3], "rb"), *marker;
		size_t length, i;
		int found = 0;
		if (message == NULL) return 24;
		length = fread(buffer, 1, sizeof(buffer), message);
		if (ferror(message) || fclose(message) != 0) return 24;
		for (i = 0; i + sizeof(expected) - 1 <= length; i++)
			if (memcmp(buffer + i, expected, sizeof(expected) - 1) == 0) found = 1;
		if (!found) return 24;
		marker = fopen(argv[2], "wb");
		if (marker == NULL) return 24;
		if (fputs("template verified\n", marker) == EOF || fclose(marker) != 0) return 24;
		return 23; /* Deliberate editor failure after checking actual template data. */
	}
#ifndef _WIN32
	/* Resource limits apply only inside the spawned private regression child. */
	if (argc >= 4 && strcmp(argv[1], "limit-exec") == 0) {
		const char *error;
		struct rlimit limit;
		long long bytes = strtonum(argv[2], 1, INT_MAX, &error);
		if (error != NULL) return 64;
		limit.rlim_cur = limit.rlim_max = (rlim_t)bytes;
		if (signal(SIGXFSZ, SIG_IGN) == SIG_ERR || setrlimit(RLIMIT_FSIZE, &limit) != 0) {
			perror("install private file-size fault injection");
			return 71;
		}
		execvp(argv[3], &argv[3]);
		perror("execute private fault-injection child");
		return 71;
	}
#endif
	if (!((argc == 4 && (strcmp(argv[1], "read") == 0 ||
	    strcmp(argv[1], "write") == 0)) ||
	    (argc == 3 && strcmp(argv[1], "cleanup") == 0))) {
		fprintf(stderr, "usage: test-fs-probe read|write UTF8-control-file data-file\n"
		    "       test-fs-probe cleanup UTF8-control-file\n");
		return 64;
	}
	target = control_path(argv[2]);
	if (target == NULL) {
		perror("read UTF-8 test control file");
		return 1;
	}
	if (strcmp(argv[1], "cleanup") == 0) {
		result = cleanup_sandbox(target) == 0 ? 0 : 1;
		if (result != 0)
			perror("remove private native test sandbox");
		free(target);
		return result;
	}
	if (strcmp(argv[1], "write") == 0) {
		if (parents(target) != 0) {
			perror("create UTF-8 test parent directories");
			goto done;
		}
		source_name = argv[3];
		destination_name = target;
	} else {
		source_name = target;
		destination_name = argv[3];
	}
	source = fopen(source_name, "rb");
	if (source == NULL) {
		perror("open test source");
		goto done;
	}
	destination = fopen(destination_name, "wb");
	if (destination == NULL) {
		perror("open test destination");
		goto done;
	}
	while ((n = fread(buffer, 1, sizeof(buffer), source)) != 0) {
		if (fwrite(buffer, 1, n, destination) != n) {
			perror("copy test bytes");
			goto done;
		}
	}
	if (ferror(source)) {
		perror("read test bytes");
		goto done;
	}
	result = 0;
 done:
	if (destination != NULL && fclose(destination) != 0)
		result = 1;
	if (source != NULL && fclose(source) != 0)
		result = 1;
	free(target);
	return result;
}
