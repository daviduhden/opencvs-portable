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
#include <sys/wait.h>

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>

#include "pc_process.h"
#include "portable.h"
extern char **environ;

int
pc_pipe(int fds[2])
{
	if (fds == NULL) {
		errno = EINVAL;
		return -1;
	}
	fds[0] = fds[1] = -1;
#ifdef PC_HAVE_PIPE2
	if (pipe2(fds, O_CLOEXEC) == 0)
		return 0;
	if (errno != ENOSYS)
		return -1;
#endif
	/* The pipe/fcntl fallback assumes the engine spawns on a single thread.
	 */
	if (pipe(fds) == -1)
		return -1;
	if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) == -1 ||
	    fcntl(fds[1], F_SETFD, FD_CLOEXEC) == -1) {
		int saved = errno;
		close(fds[0]);
		close(fds[1]);
		fds[0] = fds[1] = -1;
		errno = saved;
		return -1;
	}
	return 0;
}

int
pc_spawn(pc_process *process, const char *const argv[], int input, int output,
    int error)
{
	posix_spawn_file_actions_t actions;
	pid_t			   pid;
	char			 **mutable;
	size_t			   count = 0, copied;
	int			   result, i, j, fd[3] = {input, output, error};
	int			   duplicate[3] = {-1, -1, -1};

	if (process == NULL || argv == NULL || argv[0] == NULL ||
	    argv[0][0] == '\0') {
		errno = EINVAL;
		return -1;
	}
	process->handle = 0;
	/* Validate before allocating: a duplicate must not fill a bad source
	 * fd. */
	for (i = 0; i < 3; i++) {
		if (fd[i] < -1) {
			errno = EINVAL;
			return -1;
		}
		if (fd[i] >= 0 && fcntl(fd[i], F_GETFD) == -1)
			return -1;
	}
	while (argv[count] != NULL) {
		if (count == SIZE_MAX / sizeof(*mutable) - 1) {
			errno = E2BIG;
			return -1;
		}
		count++;
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
		pc_free_argv(mutable);
		errno = result;
		return -1;
	}
	/*
	 * Snapshot every source above stdio before any dup2 action. Otherwise
	 * mappings such as stdin <- stdout, stdout <- stdin clobber a source.
	 * POSIX.1-2008 F_DUPFD_CLOEXEC avoids introducing inheritable copies.
	 */
	for (i = 0; i < 3; i++) {
		if (fd[i] == -1)
			continue;
		do {
			duplicate[i] = fcntl(fd[i], F_DUPFD_CLOEXEC, 3);
		} while (duplicate[i] == -1 && errno == EINTR);
		if (duplicate[i] == -1) {
			result = errno;
			goto out;
		}
		result =
		    posix_spawn_file_actions_adddup2(&actions, duplicate[i], i);
		if (result != 0)
			goto out;
	}
	for (i = 0; i < 3; i++) {
		if (duplicate[i] == -1)
			continue;
		result =
		    posix_spawn_file_actions_addclose(&actions, duplicate[i]);
		if (result != 0)
			goto out;
		/* Redirect sources are borrowed in the parent, not child
		 * pass-throughs. */
		if (fd[i] <= STDERR_FILENO)
			continue;
		for (j = 0; j < i; j++)
			if (fd[j] == fd[i])
				break;
		if (j == i) {
			result =
			    posix_spawn_file_actions_addclose(&actions, fd[i]);
			if (result != 0)
				goto out;
		}
	}
	result =
	    posix_spawnp(&pid, mutable[0], &actions, NULL, mutable, environ);
out:
	posix_spawn_file_actions_destroy(&actions);
	for (i = 0; i < 3; i++)
		if (duplicate[i] != -1)
			(void)close(duplicate[i]);
	pc_free_argv(mutable);
	if (result != 0) {
		errno = result;
		return -1;
	}
	process->handle = (intptr_t)pid;
	return 0;
}

/* Reject process-group and wildcard waitpid values; this handle owns one PID.
 */
static int
pc_process_pid(const pc_process *process, pid_t *pid)
{
	if (process == NULL || process->handle <= 0) {
		errno = EINVAL;
		return -1;
	}
	*pid = (pid_t)process->handle;
	if (*pid <= 0 || (intptr_t)*pid != process->handle) {
		errno = EINVAL;
		return -1;
	}
	return 0;
}

int
pc_wait(pc_process *process, int *exit_code)
{
	int   status;
	pid_t result, pid;
	if (exit_code == NULL || pc_process_pid(process, &pid) == -1) {
		errno = EINVAL;
		return -1;
	}
	do {
		result = waitpid(pid, &status, 0);
	} while (result == -1 && errno == EINTR);
	if (result == -1) {
		if (errno == ECHILD)
			process->handle = 0;
		return -1;
	}
	process->handle = 0;
	*exit_code = WIFEXITED(status) ?
	    WEXITSTATUS(status) :
	    (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1);
	return 0;
}

/* Only registered detached children are reaped; pc_wait retains its child. */
#define PC_DETACHED_MAX 64
static volatile sig_atomic_t pc_detached[PC_DETACHED_MAX];
static int		     pc_reaper_installed;

static void
pc_reap(int signal_number)
{
	int   i, saved = errno;
	pid_t pid, result;
	(void)signal_number;
	for (i = 0; i < PC_DETACHED_MAX; i++) {
		pid = (pid_t)pc_detached[i];
		if (pid == 0)
			continue;
		do {
			result = waitpid(pid, NULL, WNOHANG);
		} while (result == -1 && errno == EINTR);
		if (result == pid || (result == -1 && errno == ECHILD))
			pc_detached[i] = 0;
	}
	errno = saved;
}

void
pc_detach(pc_process *process)
{
	struct sigaction action;
	sigset_t	 block, old;
	int		 i, result;
	pid_t		 pid;

	if (pc_process_pid(process, &pid) == -1)
		return;
	/* The signal-safe registry must never receive a truncated PID. */
	if ((uintmax_t)pid > (uintmax_t)SIG_ATOMIC_MAX) {
		(void)pc_wait(process, &result);
		return;
	}
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
		/* SIGCHLD may have arrived before the child entered the
		 * registry. */
		pc_detached[i] = (sig_atomic_t)process->handle;
		process->handle = 0;
		pc_reap(0);
	}
	(void)sigprocmask(SIG_SETMASK, &old, NULL);
	if (i == PC_DETACHED_MAX)
		(void)pc_wait(process, &result);
}
