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
/* POSIX process backend. SPDX-License-Identifier: ISC */
#include "portable.h"
#include "pc_process.h"
#include <spawn.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
extern char **environ;

int
pc_pipe(int fds[2])
{
	if (pipe(fds) == -1)
		return -1;
	if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) == -1 ||
	    fcntl(fds[1], F_SETFD, FD_CLOEXEC) == -1) {
		int saved = errno;
		close(fds[0]); close(fds[1]); errno = saved;
		return -1;
	}
	return 0;
}

int
pc_spawn(pc_process *process, const char *const argv[], int input,
    int output, int error)
{
	posix_spawn_file_actions_t actions;
	pid_t pid;
	char **mutable;
	size_t count = 0, copied = 0;
	int result, i, fd[3] = {input, output, error};

	while (argv[count] != NULL)
		count++;
	if (count == 0 || count > SIZE_MAX / sizeof(*mutable) - 1) {
		errno = EINVAL;
		return -1;
	}
	mutable = calloc(count + 1, sizeof(*mutable));
	if (mutable == NULL)
		return -1;
	for (copied = 0; copied < count; copied++) {
		mutable[copied] = strdup(argv[copied]);
		if (mutable[copied] == NULL) {
			pc_free_argv(mutable);
			return -1;
		}
	}
	result = posix_spawn_file_actions_init(&actions);
	if (result != 0) {
		pc_free_argv(mutable); errno = result; return -1;
	}
	for (i = 0; i < 3; i++) {
		if (fd[i] >= 0) {
			result = posix_spawn_file_actions_adddup2(&actions, fd[i], i);
			if (result != 0)
				goto out;
		}
	}
	result = posix_spawnp(&pid, mutable[0], &actions, NULL, mutable, environ);
out:
	posix_spawn_file_actions_destroy(&actions);
	pc_free_argv(mutable);
	if (result != 0) { errno = result; return -1; }
	process->handle = (intptr_t)pid;
	return 0;
}

int
pc_wait(pc_process *process, int *exit_code)
{
	int status;
	pid_t result;
	do { result = waitpid((pid_t)process->handle, &status, 0); }
	while (result == -1 && errno == EINTR);
	if (result == -1)
		return -1;
	process->handle = 0;
	*exit_code = WIFEXITED(status) ? WEXITSTATUS(status) :
	    (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1);
	return 0;
}

/* Only registered detached children are reaped; pc_wait retains its child. */
#define PC_DETACHED_MAX 64
static volatile sig_atomic_t pc_detached[PC_DETACHED_MAX];
static int pc_reaper_installed;

static void
pc_reap(int signal_number)
{
	int i, saved = errno;
	(void)signal_number;
	for (i = 0; i < PC_DETACHED_MAX; i++)
		if (pc_detached[i] != 0 &&
		    waitpid((pid_t)pc_detached[i], NULL, WNOHANG) ==
		    (pid_t)pc_detached[i])
			pc_detached[i] = 0;
	errno = saved;
}

void
pc_detach(pc_process *process)
{
	struct sigaction action;
	sigset_t block, old;
	int i, result;
	sigemptyset(&block);
	sigaddset(&block, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &block, &old) == -1) {
		(void)pc_wait(process, &result);
		return;
	}
	if (!pc_reaper_installed) {
		memset(&action, 0, sizeof(action));
		action.sa_handler = pc_reap;
		action.sa_flags = SA_RESTART;
		sigemptyset(&action.sa_mask);
		if (sigaction(SIGCHLD, &action, NULL) == -1) {
			(void)sigprocmask(SIG_SETMASK, &old, NULL);
			(void)pc_wait(process, &result);
			return;
		}
		pc_reaper_installed = 1;
	}
	pc_reap(0);
	for (i = 0; i < PC_DETACHED_MAX; i++)
		if (pc_detached[i] == 0)
			break;
	if (i < PC_DETACHED_MAX) {
		pc_detached[i] = (sig_atomic_t)process->handle;
		process->handle = 0;
		pc_reap(0);
	}
	(void)sigprocmask(SIG_SETMASK, &old, NULL);
	if (i == PC_DETACHED_MAX)
		(void)pc_wait(process, &result);
}
