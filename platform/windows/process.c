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
/* Native Win32 process transport. ISC license; see LICENSE. */
#include "windows_compat.h"
#include "pc_process.h"
#include <wchar.h>
#include <limits.h>

#define PC_COMMAND_LIMIT 32767

static int
append(wchar_t *out, size_t *used, wchar_t ch)
{
	if (*used >= PC_COMMAND_LIMIT - 1) { errno = E2BIG; return -1; }
	out[(*used)++] = ch; return 0;
}

/* Quote one argv item according to the Microsoft CRT argv grammar. */
static int
quote_arg(wchar_t *out, size_t *used, const wchar_t *arg)
{
	size_t slashes = 0, i;
	if (append(out, used, L'"') == -1) return -1;
	for (; *arg; arg++) {
		if (*arg == L'\\') { slashes++; continue; }
		for (i = 0; i < slashes * (*arg == L'"' ? 2u : 1u); i++)
			if (append(out, used, L'\\') == -1) return -1;
		slashes = 0;
		if (*arg == L'"' && append(out, used, L'\\') == -1) return -1;
		if (append(out, used, *arg) == -1) return -1;
	}
	for (i = 0; i < slashes * 2; i++)
		if (append(out, used, L'\\') == -1) return -1;
	return append(out, used, L'"');
}

static wchar_t *
find_program(const char *program)
{
	wchar_t *wide = pc_windows_utf16(program), *path, *full;
	DWORD len, found, error;
	if (wide == NULL) return NULL;
	if (wcschr(wide, L'/') || wcschr(wide, L'\\') || wcschr(wide, L':')) {
		free(wide); return pc_windows_path(program);
	}
	len = GetEnvironmentVariableW(L"PATH", NULL, 0);
	if (len == 0) { free(wide); errno = ENOENT; return NULL; }
	path = malloc((size_t)len * sizeof(*path));
	if (path == NULL) { free(wide); return NULL; }
	found = GetEnvironmentVariableW(L"PATH", path, len);
	if (found == 0 || found >= len) {
		if (found == 0) pc_windows_errno(GetLastError()); else errno = EAGAIN;
		free(path); free(wide); return NULL;
	}
	/* An explicit PATH avoids CreateProcess's implicit current-directory search. */
	len = SearchPathW(path, wide, L".exe", 0, NULL, NULL);
	if (len == 0 || len >= PC_COMMAND_LIMIT) {
		if (len == 0) pc_windows_errno(GetLastError()); else errno = ENAMETOOLONG;
		free(path); free(wide); return NULL;
	}
	full = malloc(((size_t)len + 1) * sizeof(*full));
	if (full == NULL) { free(path); free(wide); return NULL; }
	found = SearchPathW(path, wide, L".exe", len + 1, full, NULL);
	error = GetLastError();
	free(path); free(wide);
	if (found == 0 || found > len) {
		if (found == 0) pc_windows_errno(error); else errno = EAGAIN;
		free(full); return NULL;
	}
	return full;
}

static HANDLE
child_handle(int fd, DWORD standard, int input)
{
	HANDLE source, copy = NULL, temporary = NULL;
	if (fd >= 0) {
		intptr_t value = _get_osfhandle(fd);
		if (value == -1) { errno = EBADF; return NULL; }
		source = (HANDLE)value;
	} else source = GetStdHandle(standard);
	if (source == NULL || source == INVALID_HANDLE_VALUE) {
		temporary = CreateFileW(L"NUL", input ? GENERIC_READ : GENERIC_WRITE,
		    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (temporary == INVALID_HANDLE_VALUE) { pc_windows_errno(GetLastError()); return NULL; }
		source = temporary;
	}
	if (!DuplicateHandle(GetCurrentProcess(), source, GetCurrentProcess(), &copy,
	    0, TRUE, DUPLICATE_SAME_ACCESS)) {
		pc_windows_errno(GetLastError()); copy = NULL;
	}
	if (temporary != NULL) CloseHandle(temporary);
	return copy;
}

int
pc_spawn(pc_process *child, const char *const argv[], int input, int output,
    int error)
{
	STARTUPINFOEXW startup;
	PROCESS_INFORMATION process;
	HANDLE handles[3] = { NULL, NULL, NULL };
	wchar_t *application = NULL, *command = NULL, *arg;
	SIZE_T size = 0;
	size_t used = 0, i;
	int result = -1, saved_errno;
	BOOL initialized = FALSE;
	if (child == NULL || argv == NULL || argv[0] == NULL || argv[0][0] == '\0' ||
	    input < -1 || output < -1 || error < -1) {
		errno = EINVAL; return -1;
	}
	child->handle = 0;
	memset(&startup, 0, sizeof(startup)); memset(&process, 0, sizeof(process));
	startup.StartupInfo.cb = (DWORD)sizeof(startup);
	application = find_program(argv[0]);
	if (application == NULL) goto done;
	command = calloc(PC_COMMAND_LIMIT, sizeof(*command));
	if (command == NULL) goto done;
	for (i = 0; argv[i] != NULL; i++) {
		arg = pc_windows_utf16(argv[i]);
		if (arg == NULL) goto done;
		if ((i != 0 && append(command, &used, L' ') == -1) ||
		    quote_arg(command, &used, arg) == -1) { free(arg); goto done; }
		free(arg);
	}
	command[used] = L'\0';
	handles[0] = child_handle(input, STD_INPUT_HANDLE, 1);
	handles[1] = child_handle(output, STD_OUTPUT_HANDLE, 0);
	handles[2] = child_handle(error, STD_ERROR_HANDLE, 0);
	if (!handles[0] || !handles[1] || !handles[2]) goto done;
	startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	startup.StartupInfo.hStdInput = handles[0];
	startup.StartupInfo.hStdOutput = handles[1];
	startup.StartupInfo.hStdError = handles[2];
	(void)InitializeProcThreadAttributeList(NULL, 1, 0, &size);
	if (size == 0) { pc_windows_errno(GetLastError()); goto done; }
	startup.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, size);
	if (startup.lpAttributeList == NULL) { errno = ENOMEM; goto done; }
	if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &size)) {
		pc_windows_errno(GetLastError()); goto done;
	}
	initialized = TRUE;
	if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0,
	    PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles, sizeof(handles), NULL, NULL)) {
		pc_windows_errno(GetLastError()); goto done;
	}
	if (!CreateProcessW(application, command, NULL, NULL, TRUE,
	    EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT, NULL, NULL,
	    &startup.StartupInfo, &process)) { pc_windows_errno(GetLastError()); goto done; }
	CloseHandle(process.hThread);
	child->handle = (intptr_t)process.hProcess;
	result = 0;
done:
	saved_errno = errno;
	if (initialized) DeleteProcThreadAttributeList(startup.lpAttributeList);
	if (startup.lpAttributeList != NULL) HeapFree(GetProcessHeap(), 0, startup.lpAttributeList);
	for (i = 0; i < 3; i++) if (handles[i] != NULL) CloseHandle(handles[i]);
	free(application); free(command); errno = saved_errno;
	return result;
}

int
pc_wait(pc_process *child, int *exit_code)
{
	HANDLE handle;
	DWORD code;
	if (child == NULL || (child->handle == 0 || child->handle == -1) || exit_code == NULL) { errno = EINVAL; return -1; }
	handle = (HANDLE)child->handle;
	if (WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0 ||
	    !GetExitCodeProcess(handle, &code)) return pc_windows_errno(GetLastError());
	CloseHandle(handle); child->handle = 0;
	/* Native exception exit statuses are non-POSIX failures, never negative API success. */
	*exit_code = code <= (DWORD)INT_MAX ? (int)code : 255;
	return 0;
}

int
pc_pipe(int fds[2])
{
	return _pipe(fds, 65536, _O_BINARY | _O_NOINHERIT);
}

void
pc_detach(pc_process *child)
{
	if (child != NULL && child->handle != 0) {
		CloseHandle((HANDLE)child->handle);
		child->handle = 0;
	}
}
