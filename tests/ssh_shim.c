/*
 * Portable OpenCVS test transport. This is a local process proxy, not SSH.
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
#include "portable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_process.h"

int
main(int argc, char **argv)
{
	pc_process child;
	int argument = 1, status;
	char **parsed = NULL;
	const char *configured;
	const char *const *command;

	pc_platform_startup(&argc, &argv);
	if (argument < argc && strcmp(argv[argument], "-l") == 0) {
		if (argument + 1 >= argc)
			return 64;
		argument += 2;
	}
	if (argument < argc && strcmp(argv[argument], "--") == 0)
		argument++;
	if (argument >= argc || strcmp(argv[argument], "test.invalid") != 0) {
		fprintf(stderr, "test transport requires host test.invalid\n");
		return 64;
	}
	argument++;
	/* GNU CVS supplies one remote-command argument; OpenCVS supplies argv. */
	if (argc - argument == 1) {
		parsed = pc_command_argv(argv[argument], NULL);
		command = (const char *const *)parsed;
	} else if (argc - argument == 2) {
		command = (const char *const *)&argv[argument];
	} else {
		command = NULL;
	}
	configured = getenv("CVS_SERVER");
	if (command == NULL || command[0] == NULL || command[1] == NULL ||
	    command[2] != NULL || configured == NULL ||
	    strcmp(command[0], configured) != 0 || strcmp(command[1], "server") != 0) {
		fprintf(stderr, "test transport requires an executable and server\n");
		pc_free_argv(parsed);
		return 64;
	}
	if (pc_spawn(&child, command, -1, -1, -1) != 0) {
		perror("test transport spawn");
		pc_free_argv(parsed);
		return 71;
	}
	pc_free_argv(parsed);
	if (pc_wait(&child, &status) != 0) {
		perror("test transport wait");
		return 71;
	}
	return status;
}
