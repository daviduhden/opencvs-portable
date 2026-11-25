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
/* Portable OpenCVS process abstraction. SPDX-License-Identifier: ISC */
#ifndef PC_PROCESS_H
#define PC_PROCESS_H
#include <stdint.h>
/* Backend-owned child token. A handle of zero means no child is held. */
typedef struct pc_process {
	intptr_t handle;
} pc_process;
/*
 * Spawn takes a NULL-terminated argv and borrows the caller's descriptors.
 * A stream argument of -1 selects the caller's corresponding standard
 * stream. Launch bypasses the shell.
 */
int pc_spawn(pc_process *, const char *const[], int, int, int);
/* A successful wait consumes the token and reports child status separately. */
int pc_wait(pc_process *, int *);
/* Relinquish the child token. POSIX can wait if it cannot arrange reaping. */
void pc_detach(pc_process *);
/* Return caller-owned read and write descriptors, in that order. */
int pc_pipe(int[2]);
int pc_run(const char *const[]);
/*
 * Parse words with quoting and escaping. If provided, append the last
 * argument verbatim as one word. Free the result with pc_free_argv.
 */
char **pc_command_argv(const char *, const char *);
void   pc_free_argv(char **);
#endif
