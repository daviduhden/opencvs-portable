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
/* Portable OpenCVS native Windows filesystem. ISC license; see LICENSE. */
#include "windows_compat.h"
#include <aclapi.h>
#include <bcrypt.h>
#include <sddl.h>
#include <stdarg.h>
#include <limits.h>
#include <wchar.h>
#include <dirent.h>

static mode_t creation_mask = 0022;

mode_t
pc_umask(mode_t mask)
{
	mode_t previous = creation_mask;
	creation_mask = (mode_t)(mask & 0777);
	return previous;
}


int
pc_windows_errno(DWORD error)
{
	switch (error) {
	case ERROR_SUCCESS: return 0;
	case ERROR_FILE_NOT_FOUND: case ERROR_PATH_NOT_FOUND: errno = ENOENT; break;
	case ERROR_ACCESS_DENIED: case ERROR_PRIVILEGE_NOT_HELD: errno = EACCES; break;
	case ERROR_FILE_EXISTS: case ERROR_ALREADY_EXISTS: errno = EEXIST; break;
	case ERROR_SHARING_VIOLATION: case ERROR_LOCK_VIOLATION: errno = EBUSY; break;
	case ERROR_NOT_ENOUGH_MEMORY: case ERROR_OUTOFMEMORY: errno = ENOMEM; break;
	case ERROR_INVALID_HANDLE: errno = EBADF; break;
	case ERROR_INVALID_NAME: case ERROR_BAD_PATHNAME: errno = EINVAL; break;
	case ERROR_FILENAME_EXCED_RANGE: errno = ENAMETOOLONG; break;
	case ERROR_DIRECTORY: errno = ENOTDIR; break;
	case ERROR_DIR_NOT_EMPTY: errno = ENOTEMPTY; break;
	case ERROR_BROKEN_PIPE: case ERROR_NO_DATA: errno = EPIPE; break;
	case ERROR_DISK_FULL: errno = ENOSPC; break;
	case ERROR_NOT_SAME_DEVICE: errno = EXDEV; break;
	case ERROR_NO_UNICODE_TRANSLATION: errno = EILSEQ; break;
	case ERROR_BAD_EXE_FORMAT: errno = ENOEXEC; break;
	case ERROR_OPERATION_ABORTED: errno = EINTR; break;
	default: errno = EIO; break;
	}
	return -1;
}

wchar_t *
pc_windows_utf16(const char *text)
{
	int len;
	wchar_t *out;
	if (text == NULL) { errno = EINVAL; return NULL; }
	len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
	    NULL, 0);
	if (len == 0) { pc_windows_errno(GetLastError()); return NULL; }
	out = malloc((size_t)len * sizeof(*out));
	if (out == NULL) return NULL;
	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1,
	    out, len)) { pc_windows_errno(GetLastError()); free(out); return NULL; }
	return out;
}

char *
pc_windows_utf8(const wchar_t *text)
{
	int len;
	char *out;
	if (text == NULL) { errno = EINVAL; return NULL; }
	len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1,
	    NULL, 0, NULL, NULL);
	if (len == 0) { pc_windows_errno(GetLastError()); return NULL; }
	out = malloc((size_t)len);
	if (out == NULL) return NULL;
	if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1,
	    out, len, NULL, NULL)) { pc_windows_errno(GetLastError()); free(out); return NULL; }
	return out;
}

static int
valid_component(const wchar_t *name, size_t length)
{
	static const wchar_t *const reserved[] = { L"CON", L"PRN", L"AUX", L"NUL", L"CLOCK$", L"CONIN$", L"CONOUT$" };
	size_t i, base = length;
	if (length == 0 || (length == 1 && name[0] == L'.') ||
	    (length == 2 && name[0] == L'.' && name[1] == L'.')) return 1;
	if (name[length - 1] == L'.' || name[length - 1] == L' ') return 0;
	for (i = 0; i < length; i++) {
		if (name[i] < 32 || wcschr(L"<>:\"|?*", name[i]) != NULL) return 0;
		if (name[i] == L'.' && base == length) base = i;
	}
	for (i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++)
		if (wcslen(reserved[i]) == base && CompareStringOrdinal(name, (int)base, reserved[i], (int)base, TRUE) == CSTR_EQUAL) return 0;
	if (base == 4 && (CompareStringOrdinal(name, 3, L"COM", 3, TRUE) == CSTR_EQUAL ||
	    CompareStringOrdinal(name, 3, L"LPT", 3, TRUE) == CSTR_EQUAL) &&
	    ((name[3] >= L'1' && name[3] <= L'9') || name[3] == 0x00b9 || name[3] == 0x00b2 || name[3] == 0x00b3)) return 0;
	return 1;
}

static int
valid_native_path(const wchar_t *path)
{
	const wchar_t *part = path, *end;
	if (((part[0] >= L'A' && part[0] <= L'Z') || (part[0] >= L'a' && part[0] <= L'z')) && part[1] == L':') {
		if (part[2] != L'\\') return 0;
		part += 3;
	}
	for (;;) {
		end = wcschr(part, L'\\');
		if (!valid_component(part, end != NULL ? (size_t)(end - part) : wcslen(part))) return 0;
		if (end == NULL) return 1;
		part = end + 1;
	}
}

/* Strict UTF-8 input, absolute extended-length output; never ANSI APIs. */
wchar_t *
pc_windows_path(const char *path)
{
	wchar_t *input, *full, *out;
	DWORD len, got, error;
	size_t i, count, prefix;
	input = pc_windows_utf16(path);
	if (input == NULL) return NULL;
	for (i = 0; input[i] != L'\0'; i++)
		if (input[i] == L'/') input[i] = L'\\';
	if (!wcsncmp(input, L"\\\\?\\", 4) || !wcsncmp(input, L"\\\\.\\", 4)) {
		free(input); errno = EINVAL; return NULL;
	}
	if (!valid_native_path(input)) { free(input); errno = EINVAL; return NULL; }
	len = GetFullPathNameW(input, 0, NULL, NULL);
	if (len == 0 || len > 32760) {
		if (len == 0) pc_windows_errno(GetLastError()); else errno = ENAMETOOLONG;
		free(input); return NULL;
	}
	full = malloc(((size_t)len + 1) * sizeof(*full));
	if (full == NULL) { free(input); return NULL; }
	got = GetFullPathNameW(input, len + 1, full, NULL);
	error = GetLastError();
	free(input);
	if (got == 0 || got > len) {
		if (got == 0) pc_windows_errno(error); else errno = EAGAIN;
		free(full); return NULL;
	}
	count = wcslen(full);
	while (count > 3 && full[count - 1] == L'\\') full[--count] = L'\0';
	prefix = !wcsncmp(full, L"\\\\", 2) ? 6 : 4;
	out = malloc((count + prefix + 1) * sizeof(*out));
	if (out == NULL) { free(full); return NULL; }
	if (prefix == 6) {
		memcpy(out, L"\\\\?\\UNC\\", 8 * sizeof(*out));
		memcpy(out + 8, full + 2, (count - 1) * sizeof(*out));
	} else {
		memcpy(out, L"\\\\?\\", 4 * sizeof(*out));
		memcpy(out + 4, full, (count + 1) * sizeof(*out));
	}
	free(full);
	return out;
}

static int
check_components(wchar_t *path)
{
	wchar_t *p;
	DWORD attrs;
	/* Start past drive root or UNC server/share prefix. */
	p = path + 4;
	if (!wcsncmp(path, L"\\\\?\\UNC\\", 8)) {
		p = wcschr(path + 8, L'\\');
		if (p == NULL) { errno = EINVAL; return -1; }
		p = wcschr(p + 1, L'\\');
		if (p == NULL) return 0;
	} else if (p[0] && p[1] == L':' && p[2] == L'\\') p += 3;
	for (; *p; p++) {
		if (*p != L'\\') continue;
		*p = L'\0'; attrs = GetFileAttributesW(path); *p = L'\\';
		if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) {
			errno = ELOOP; return -1;
		}
	}
	return 0;
}

static int
check_handle(HANDLE handle, const wchar_t *expected)
{
	BY_HANDLE_FILE_INFORMATION info;
	wchar_t *actual, *canonical;
	WIN32_FIND_DATAW entry;
	HANDLE search;
	const wchar_t *base, *unc_share = NULL;
	DWORD len, got;
	if (!GetFileInformationByHandle(handle, &info)) return pc_windows_errno(GetLastError());
	if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { errno = ELOOP; return -1; }
	len = GetFinalPathNameByHandleW(handle, NULL, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	if (len == 0) return pc_windows_errno(GetLastError());
	actual = malloc(((size_t)len + 1) * sizeof(*actual));
	if (actual == NULL) return -1;
	got = GetFinalPathNameByHandleW(handle, actual, len + 1, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
	if (got == 0 || got > len) {
		if (got == 0) pc_windows_errno(GetLastError()); else errno = EAGAIN;
		free(actual); return -1;
	}
	len = GetLongPathNameW(expected, NULL, 0);
	if (len == 0) { pc_windows_errno(GetLastError()); free(actual); return -1; }
	canonical = malloc(((size_t)len + 1) * sizeof(*canonical));
	if (canonical == NULL) { free(actual); return -1; }
	got = GetLongPathNameW(expected, canonical, len + 1);
	if (got == 0 || got > len || CompareStringOrdinal(actual, -1, canonical, -1, TRUE) != CSTR_EQUAL) {
		free(canonical); free(actual); errno = ELOOP; return -1;
	}
	free(canonical); free(actual);
	base = wcsrchr(expected, L'\\'); base = base ? base + 1 : expected;
	/* Case-only or 8.3 final-name aliases must not overwrite another CVS name. */
	if (wcsncmp(expected, L"\\\\?\\UNC\\", 8) == 0) {
		unc_share = wcschr(expected + 8, L'\\');
		if (unc_share == NULL) { errno = EINVAL; return -1; }
		unc_share++;
	}
	if (*base != L'\0' && !(unc_share != NULL && wcschr(unc_share, L'\\') == NULL)) {
		search = FindFirstFileW(expected, &entry);
		if (search == INVALID_HANDLE_VALUE) return pc_windows_errno(GetLastError());
		FindClose(search);
		if (wcscmp(entry.cFileName, base)) {
			/* Native environment paths may name a directory by its verified
			 * 8.3 alias (for example RUNNER~1). Never allow file aliases. */
			if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
			    entry.cAlternateFileName[0] == L'\0' ||
			    CompareStringOrdinal(entry.cAlternateFileName, -1, base, -1, TRUE) != CSTR_EQUAL) {
				errno = EEXIST; return -1;
			}
		}
	}
	return 0;
}

static int
check_named(const wchar_t *path)
{
	HANDLE h;
	int result;
	h = CreateFileW(path, FILE_READ_ATTRIBUTES,
	    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
	    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h == INVALID_HANDLE_VALUE) return pc_windows_errno(GetLastError());
	result = check_handle(h, path);
	{
		DWORD error = GetLastError();
		CloseHandle(h); SetLastError(error);
	}
	return result;
}

int
pc_open(const char *path, int flags, ...)
{
	wchar_t *wide;
	HANDLE handle;
	DWORD access, creation, attrs;
	SECURITY_ATTRIBUTES sa;
	PSECURITY_DESCRIPTOR private_sd = NULL;
	int fd, crtflags;
	mode_t mode = 0666;
	va_list ap;
	if (flags & O_CREAT) { va_start(ap, flags); mode = (mode_t)va_arg(ap, int); va_end(ap); mode = (mode_t)(mode & ~creation_mask); }
	/* NUL is an intentional native null-device mapping, not a repo name. */
	if (!strcmp(path, "/dev/null")) {
		access = (flags & O_RDWR) ? GENERIC_READ | GENERIC_WRITE :
		    ((flags & O_WRONLY) ? GENERIC_WRITE : GENERIC_READ);
		handle = CreateFileW(L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE,
		    NULL, OPEN_EXISTING, 0, NULL);
		if (handle == INVALID_HANDLE_VALUE) return pc_windows_errno(GetLastError());
		fd = _open_osfhandle((intptr_t)handle,
		    (flags & (O_RDONLY | O_WRONLY | O_RDWR)) | _O_BINARY | _O_NOINHERIT);
		if (fd == -1) CloseHandle(handle);
		return fd;
	}
	wide = pc_windows_path(path);
	if (wide == NULL) return -1;
	if (check_components(wide) == -1) { free(wide); return -1; }
	access = (flags & O_RDWR) ? GENERIC_READ | GENERIC_WRITE :
	    ((flags & O_WRONLY) ? GENERIC_WRITE : GENERIC_READ);
	creation = (flags & O_CREAT) ? ((flags & O_EXCL) ? CREATE_NEW : OPEN_ALWAYS) : OPEN_EXISTING;
	/* A real directory HANDLE is needed by the core file walker and fstat. */
	attrs = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
	memset(&sa, 0, sizeof(sa)); sa.nLength = (DWORD)sizeof(sa);
	if ((flags & O_CREAT) && !(mode & 0077)) {
		if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
		    L"D:P(A;;FA;;;OW)(A;;FA;;;SY)", SDDL_REVISION_1, &private_sd, NULL)) {
			pc_windows_errno(GetLastError()); free(wide); return -1;
		}
		sa.lpSecurityDescriptor = private_sd;
	}
	handle = CreateFileW(wide, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	    private_sd ? &sa : NULL, creation, attrs, NULL);
	if (private_sd != NULL) { DWORD error = GetLastError(); LocalFree(private_sd); SetLastError(error); }
	if (handle == INVALID_HANDLE_VALUE) { pc_windows_errno(GetLastError()); free(wide); return -1; }
	if (check_handle(handle, wide) == -1) { CloseHandle(handle); free(wide); return -1; }
	free(wide);
	if (flags & O_TRUNC) {
		LARGE_INTEGER zero; zero.QuadPart = 0;
		if (!SetFilePointerEx(handle, zero, NULL, FILE_BEGIN) || !SetEndOfFile(handle)) {
			pc_windows_errno(GetLastError()); CloseHandle(handle); return -1;
		}
	}
	crtflags = (flags & (O_RDONLY | O_WRONLY | O_RDWR | O_APPEND)) | _O_BINARY | _O_NOINHERIT;
	fd = _open_osfhandle((intptr_t)handle, crtflags);
	if (fd == -1) CloseHandle(handle);
	return fd;
}

static int
parse_mode(const char *mode, int *flags, char output[8])
{
	int plus;
	if (mode == NULL || mode[0] == '\0') { errno = EINVAL; return -1; }
	plus = strchr(mode, '+') != NULL;
	switch (mode[0]) {
	case 'r': *flags = plus ? O_RDWR : O_RDONLY; break;
	case 'w': *flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC; break;
	case 'a': *flags = (plus ? O_RDWR : O_WRONLY) | O_CREAT | O_APPEND; break;
	default: errno = EINVAL; return -1;
	}
	output[0] = mode[0]; output[1] = 'b'; output[2] = plus ? '+' : '\0'; output[3] = '\0';
	return 0;
}

FILE *
pc_fopen(const char *path, const char *mode)
{
	int fd, flags;
	char binary[8];
	FILE *stream;
	if (parse_mode(mode, &flags, binary) == -1) return NULL;
	fd = pc_open(path, flags, 0666);
	if (fd == -1) return NULL;
	stream = _fdopen(fd, binary);
	if (stream == NULL) _close(fd);
	return stream;
}

FILE *
pc_fdopen(int fd, const char *mode)
{
	int flags;
	char binary[8];
	if (parse_mode(mode, &flags, binary) == -1) return NULL;
	if (_setmode(fd, _O_BINARY) == -1) return NULL;
	return _fdopen(fd, binary);
}

static struct timespec
from_filetime(FILETIME value)
{
	ULARGE_INTEGER ticks;
	struct timespec out;
	ticks.LowPart = value.dwLowDateTime; ticks.HighPart = value.dwHighDateTime;
	if (ticks.QuadPart >= UINT64_C(116444736000000000)) {
		ticks.QuadPart -= UINT64_C(116444736000000000);
		out.tv_sec = (time_t)(ticks.QuadPart / UINT64_C(10000000));
		out.tv_nsec = (long)((ticks.QuadPart % UINT64_C(10000000)) * 100);
	} else {
		uint64_t diff = UINT64_C(116444736000000000) - ticks.QuadPart;
		out.tv_sec = -(time_t)((diff + UINT64_C(9999999)) / UINT64_C(10000000));
		out.tv_nsec = (long)((UINT64_C(10000000) - diff % UINT64_C(10000000)) % UINT64_C(10000000) * 100);
	}
	return out;
}

static int
stat_handle(HANDLE handle, struct pc_stat *st)
{
	BY_HANDLE_FILE_INFORMATION info;
	PSECURITY_DESCRIPTOR desc = NULL;
	PSID owner = NULL;
	DWORD error;
	memset(st, 0, sizeof(*st));
	if (GetFileType(handle) == FILE_TYPE_CHAR || GetFileType(handle) == FILE_TYPE_PIPE) {
		st->st_mode = (mode_t)((GetFileType(handle) == FILE_TYPE_CHAR ? S_IFCHR : S_IFIFO) | 0666);
		st->st_uid = UINT32_MAX; st->st_gid = UINT32_MAX;
		return 0;
	}
	if (!GetFileInformationByHandle(handle, &info)) return pc_windows_errno(GetLastError());
	if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { errno = ELOOP; return -1; }
	st->st_dev = info.dwVolumeSerialNumber;
	st->st_ino = ((uint64_t)info.nFileIndexHigh << 32) | info.nFileIndexLow;
	st->st_nlink = info.nNumberOfLinks;
	st->st_mode = (mode_t)((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? S_IFDIR | 0777 : S_IFREG | 0666);
	if (info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) st->st_mode &= (mode_t)~0222;
	st->st_uid = UINT32_MAX; st->st_gid = UINT32_MAX;
	error = GetSecurityInfo(handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &owner, NULL, NULL, NULL, &desc);
	if (error == ERROR_SUCCESS && owner != NULL) {
		unsigned char *s = owner; DWORD i, len = GetLengthSid(owner); uint32_t hash = UINT32_C(2166136261);
		for (i = 0; i < len; i++) hash = (hash ^ s[i]) * UINT32_C(16777619);
		st->st_uid = hash;
	}
	if (desc != NULL) LocalFree(desc);
	if (info.nFileSizeHigh & UINT32_C(0x80000000)) { errno = EOVERFLOW; return -1; }
	st->st_size = (pc_off_t)(((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow);
	st->st_blksize = 4096;
	st->st_atim = from_filetime(info.ftLastAccessTime);
	st->st_mtim = from_filetime(info.ftLastWriteTime);
	st->st_ctim = from_filetime(info.ftCreationTime);
	return 0;
}

int
pc_fstat(int fd, struct pc_stat *st)
{
	intptr_t raw = _get_osfhandle(fd);
	if (raw == -1) { errno = EBADF; return -1; }
	return stat_handle((HANDLE)raw, st);
}

int
pc_stat(const char *path, struct pc_stat *st)
{
	wchar_t *wide = pc_windows_path(path);
	HANDLE handle;
	int result;
	if (wide == NULL) return -1;
	if (check_components(wide) == -1) { free(wide); return -1; }
	handle = CreateFileW(wide, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
	if (handle == INVALID_HANDLE_VALUE) { pc_windows_errno(GetLastError()); free(wide); return -1; }
	result = check_handle(handle, wide);
	if (result == 0) result = stat_handle(handle, st);
	CloseHandle(handle); free(wide); return result;
}

int
pc_lstat(const char *path, struct pc_stat *st)
{
	/* Reparse points fail explicitly; they are never followed silently. */
	return pc_stat(path, st);
}

static HANDLE
clear_readonly(const wchar_t *path, FILE_BASIC_INFO *original, int *changed)
{
	DWORD attrs = GetFileAttributesW(path);
	FILE_BASIC_INFO writable;
	HANDLE h;
	*changed = 0;
	if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_READONLY)) return INVALID_HANDLE_VALUE;
	if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) { errno = ELOOP; return NULL; }
	h = CreateFileW(path, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
	    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
	    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (h == INVALID_HANDLE_VALUE) { pc_windows_errno(GetLastError()); return NULL; }
	if (!GetFileInformationByHandleEx(h, FileBasicInfo, original, (DWORD)sizeof(*original))) {
		DWORD error = GetLastError();
		pc_windows_errno(error); CloseHandle(h); SetLastError(error); return NULL;
	}
	writable = *original; writable.FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
	if (writable.FileAttributes == 0)
		writable.FileAttributes = FILE_ATTRIBUTE_NORMAL;
	if (!SetFileInformationByHandle(h, FileBasicInfo, &writable, (DWORD)sizeof(writable))) {
		DWORD error = GetLastError();
		pc_windows_errno(error); CloseHandle(h); SetLastError(error); return NULL;
	}
	*changed = 1; return h;
}

static int
restore_readonly(HANDLE h, const FILE_BASIC_INFO *original, int changed)
{
	int saved = errno, result = 0;
	DWORD error = GetLastError();
	if (h != INVALID_HANDLE_VALUE && h != NULL) {
		if (changed && !SetFileInformationByHandle(h, FileBasicInfo,
		    (void *)original, (DWORD)sizeof(*original))) {
			error = GetLastError();
			result = pc_windows_errno(error);
		}
		CloseHandle(h);
	}
	if (result == 0) errno = saved;
	SetLastError(error);
	return result;
}

int
pc_unlink(const char *path)
{
	wchar_t *wide = pc_windows_path(path);
	FILE_BASIC_INFO original;
	HANDLE h;
	int result, changed;
	if (wide == NULL) return -1;
	if (check_components(wide) == -1) { free(wide); return -1; }
	if (check_named(wide) == -1) { free(wide); return -1; }
	h = clear_readonly(wide, &original, &changed);
	if (h == NULL) { free(wide); return -1; }
	result = DeleteFileW(wide) ? 0 : pc_windows_errno(GetLastError());
	restore_readonly(h, &original, changed);
	free(wide); return result;
}

#ifdef _MSC_VER
static __declspec(thread) const char *rename_error_stage = "not called";
#else
static __thread const char *rename_error_stage = "not called";
#endif

const char *
pc_windows_rename_error_stage(void)
{
	return rename_error_stage;
}

/* Some MinGW SDKs expose FileRenameInfoEx but omit this documented flag. */
#ifndef FILE_RENAME_FLAG_IGNORE_READONLY_ATTRIBUTE
#define FILE_RENAME_FLAG_IGNORE_READONLY_ATTRIBUTE 0x00000040
#endif

int
pc_rename(const char *from, const char *to)
{
	wchar_t *a = NULL, *b = NULL, *parent = NULL, *split;
	FILE_RENAME_INFO *information = NULL;
	BY_HANDLE_FILE_INFORMATION source_info;
	HANDLE source = INVALID_HANDLE_VALUE, directory = INVALID_HANDLE_VALUE;
	DWORD attrs, native_error = ERROR_SUCCESS, access;
	size_t name_bytes, information_bytes;
	int result = -1, saved = 0, destination_absent = 0;
	const char *stage = "source path";

	SetLastError(ERROR_SUCCESS);
	a = pc_windows_path(from);
	if (a == NULL) goto failed;
	stage = "destination path";
	b = pc_windows_path(to);
	if (b == NULL) goto failed;
	stage = "source ancestor validation";
	if (check_components(a) == -1) goto failed;
	stage = "destination ancestor validation";
	if (check_components(b) == -1) goto failed;
	stage = "source rename handle";
	source = CreateFileW(a, DELETE | FILE_READ_ATTRIBUTES,
	    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
	    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (source == INVALID_HANDLE_VALUE) goto native_failure;
	stage = "source identity validation";
	if (check_handle(source, a) == -1) goto failed;
	if (!GetFileInformationByHandle(source, &source_info)) goto native_failure;
	stage = "destination identity validation";
	attrs = GetFileAttributesW(b);
	if (attrs != INVALID_FILE_ATTRIBUTES) {
		if (check_named(b) == -1) goto failed;
	} else {
		native_error = GetLastError();
		if (native_error != ERROR_FILE_NOT_FOUND && native_error != ERROR_PATH_NOT_FOUND)
			goto native_failure_saved;
		destination_absent = 1;
	}
	stage = "destination parent path";
	parent = _wcsdup(b);
	if (parent == NULL) { errno = ENOMEM; SetLastError(ERROR_SUCCESS); goto failed; }
	split = wcsrchr(parent, L'\\');
	if (split == NULL || split[1] == L'\0') {
		errno = EINVAL; SetLastError(ERROR_SUCCESS); goto failed;
	}
	name_bytes = wcslen(split + 1) * sizeof(wchar_t);
	information_bytes = offsetof(FILE_RENAME_INFO, FileName) + name_bytes + sizeof(wchar_t);
	if (information_bytes > UINT32_MAX) {
		errno = EOVERFLOW; SetLastError(ERROR_SUCCESS); goto failed;
	}
	information = calloc(1, information_bytes);
	if (information == NULL) { errno = ENOMEM; SetLastError(ERROR_SUCCESS); goto failed; }
	memcpy(information->FileName, split + 1, name_bytes);
	information->FileNameLength = (DWORD)name_bytes;
	/* Keep the slash of a native drive root, but strip other trailing slashes. */
	if (split == parent + 6 && parent[5] == L':') split[1] = L'\0';
	else *split = L'\0';
	access = FILE_READ_ATTRIBUTES | ((source_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ?
	    FILE_ADD_SUBDIRECTORY : FILE_ADD_FILE);
	stage = "destination parent handle";
	directory = CreateFileW(parent, access,
	    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
	    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (directory == INVALID_HANDLE_VALUE) goto native_failure;
	stage = "destination parent identity validation";
	if (check_handle(directory, parent) == -1) goto failed;
	information->RootDirectory = directory;
	information->Flags = FILE_RENAME_FLAG_REPLACE_IF_EXISTS |
	    FILE_RENAME_FLAG_POSIX_SEMANTICS | FILE_RENAME_FLAG_IGNORE_READONLY_ATTRIBUTE;
	stage = "SetFileInformationByHandle(FileRenameInfoEx)";
	if (SetFileInformationByHandle(source, FileRenameInfoEx, information, (DWORD)information_bytes)) {
		result = 0; goto cleanup;
	}
	native_error = GetLastError();
	if (native_error == ERROR_INVALID_PARAMETER || native_error == ERROR_NOT_SUPPORTED ||
	    native_error == ERROR_INVALID_FUNCTION) {
		/* A non-replacing fallback is safe even if another process creates
		 * the destination after our check: it must fail rather than overwrite. */
		if (destination_absent) {
			information->Flags = 0;
			stage = "SetFileInformationByHandle(FileRenameInfo, no replacement)";
			if (SetFileInformationByHandle(source, FileRenameInfo, information, (DWORD)information_bytes)) {
				result = 0; goto cleanup;
			}
			goto native_failure;
		}
		errno = ENOTSUP; saved = errno; goto cleanup;
	}
	goto native_failure_saved;
native_failure:
	native_error = GetLastError();
native_failure_saved:
	(void)pc_windows_errno(native_error); saved = errno; goto cleanup;
failed:
	native_error = GetLastError(); saved = errno;
cleanup:
	if (directory != INVALID_HANDLE_VALUE) CloseHandle(directory);
	if (source != INVALID_HANDLE_VALUE) CloseHandle(source);
	free(information); free(parent); free(a); free(b);
	rename_error_stage = result == 0 ? "success" : stage;
	if (result != 0) { errno = saved; SetLastError(native_error); }
	return result;
}

int
pc_mkdir(const char *path, mode_t mode)
{
	wchar_t *wide = pc_windows_path(path);
	SECURITY_ATTRIBUTES sa;
	PSECURITY_DESCRIPTOR private_sd = NULL;
	int result;
	if (wide == NULL) return -1;
	if (check_components(wide) == -1) { free(wide); return -1; }
	mode = (mode_t)(mode & ~creation_mask);
	memset(&sa, 0, sizeof(sa)); sa.nLength = (DWORD)sizeof(sa);
	if (!(mode & 0077)) {
		if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
		    L"D:P(A;OICI;FA;;;OW)(A;OICI;FA;;;SY)", SDDL_REVISION_1, &private_sd, NULL)) {
			pc_windows_errno(GetLastError()); free(wide); return -1;
		}
		sa.lpSecurityDescriptor = private_sd;
	}
	result = CreateDirectoryW(wide, private_sd ? &sa : NULL) ? 0 : pc_windows_errno(GetLastError());
	if (private_sd != NULL) LocalFree(private_sd);
	free(wide); return result;
}

int
pc_rmdir(const char *path)
{
	wchar_t *wide = pc_windows_path(path);
	int result;
	if (wide == NULL) return -1;
	if (check_components(wide) == -1) { free(wide); return -1; }
	if (check_named(wide) == -1) { free(wide); return -1; }
	result = RemoveDirectoryW(wide) ? 0 : pc_windows_errno(GetLastError());
	free(wide); return result;
}

int
pc_chdir(const char *path)
{
	wchar_t *wide = pc_windows_path(path);
	int result;
	if (wide == NULL) return -1;
	if (check_components(wide) == -1) { free(wide); return -1; }
	if (check_named(wide) == -1) { free(wide); return -1; }
	result = SetCurrentDirectoryW(wide) ? 0 : pc_windows_errno(GetLastError());
	free(wide); return result;
}

char *
pc_getcwd(char *buffer, size_t size)
{
	DWORD len = GetCurrentDirectoryW(0, NULL), got;
	wchar_t *wide;
	char *utf8;
	size_t bytes;
	if (len == 0) { pc_windows_errno(GetLastError()); return NULL; }
	wide = malloc((size_t)len * sizeof(*wide));
	if (wide == NULL) return NULL;
	got = GetCurrentDirectoryW(len, wide);
	if (got == 0 || got >= len) {
		if (got == 0) pc_windows_errno(GetLastError()); else errno = EAGAIN;
		free(wide); return NULL;
	}
	utf8 = pc_windows_utf8(wide); free(wide);
	if (utf8 == NULL) return NULL;
	for (char *p = utf8; *p; p++) if (*p == '\\') *p = '/';
	bytes = strlen(utf8) + 1;
	if (buffer == NULL) {
		if (size != 0 && bytes > size) { free(utf8); errno = ERANGE; return NULL; }
		return utf8;
	}
	if (bytes > size) { free(utf8); errno = ERANGE; return NULL; }
	memcpy(buffer, utf8, bytes); free(utf8); return buffer;
}

int
pc_chmod(const char *path, mode_t mode)
{
	wchar_t *wide = pc_windows_path(path);
	DWORD attrs;
	int result;
	if (wide == NULL) return -1;
	if (check_components(wide) == -1) { free(wide); return -1; }
	if (check_named(wide) == -1) { free(wide); return -1; }
	attrs = GetFileAttributesW(wide);
	if (attrs == INVALID_FILE_ATTRIBUTES) { free(wide); return pc_windows_errno(GetLastError()); }
	if (attrs & FILE_ATTRIBUTE_REPARSE_POINT) { free(wide); errno = ELOOP; return -1; }
	if (!(attrs & FILE_ATTRIBUTE_DIRECTORY) && (mode & 0111)) { free(wide); errno = ENOTSUP; return -1; }
	attrs = (mode & 0222) ? attrs & ~FILE_ATTRIBUTE_READONLY : attrs | FILE_ATTRIBUTE_READONLY;
	result = SetFileAttributesW(wide, attrs) ? 0 : pc_windows_errno(GetLastError());
	free(wide); return result;
}

int
pc_fchmod(int fd, mode_t mode)
{
	HANDLE original = (HANDLE)_get_osfhandle(fd), h;
	FILE_BASIC_INFO info;
	int result;
	if (original == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
	h = ReOpenFile(original, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
	    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
	if (h == INVALID_HANDLE_VALUE) return pc_windows_errno(GetLastError());
	if (!GetFileInformationByHandleEx(h, FileBasicInfo, &info, (DWORD)sizeof(info))) {
		pc_windows_errno(GetLastError()); CloseHandle(h); return -1;
	}
	if (info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { CloseHandle(h); errno = ELOOP; return -1; }
	if (!(info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (mode & 0111)) { CloseHandle(h); errno = ENOTSUP; return -1; }
	info.FileAttributes = (mode & 0222) ? info.FileAttributes & ~FILE_ATTRIBUTE_READONLY : info.FileAttributes | FILE_ATTRIBUTE_READONLY;
	result = SetFileInformationByHandle(h, FileBasicInfo, &info, (DWORD)sizeof(info)) ? 0 : pc_windows_errno(GetLastError());
	CloseHandle(h); return result;
}

int
pc_access(const char *path, int mode)
{
	struct pc_stat st;
	if (pc_stat(path, &st) == -1) return -1;
	if ((mode & W_OK) && !(st.st_mode & 0222)) { errno = EACCES; return -1; }
	if (mode & X_OK) { errno = ENOTSUP; return -1; }
	return 0;
}

int
pc_mkstemp(char *template)
{
	static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	size_t len = strlen(template);
	unsigned char random[6];
	unsigned int attempt;
	int fd;
	if (len < 6 || strcmp(template + len - 6, "XXXXXX")) { errno = EINVAL; return -1; }
	for (attempt = 0; attempt < 128; attempt++) {
		if (BCryptGenRandom(NULL, random, (ULONG)sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) { errno = EIO; return -1; }
		for (size_t i = 0; i < 6; i++) template[len - 6 + i] = alphabet[random[i] % (sizeof(alphabet) - 1)];
		fd = pc_open(template, O_CREAT | O_EXCL | O_RDWR, 0600);
		if (fd != -1 || errno != EEXIST) return fd;
	}
	errno = EEXIST; return -1;
}

int
pc_futimens(int fd, const struct timespec times[2])
{
	HANDLE h = (HANDLE)_get_osfhandle(fd);
	FILETIME values[2];
	ULARGE_INTEGER ticks;
	if (h == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
	if (times == NULL) {
		GetSystemTimeAsFileTime(&values[0]);
		return SetFileTime(h, NULL, &values[0], &values[0]) ? 0 : pc_windows_errno(GetLastError());
	}
	for (int i = 0; i < 2; i++) {
		if (times[i].tv_nsec < 0 || times[i].tv_nsec >= 1000000000L || times[i].tv_sec < (time_t)-11644473600LL || times[i].tv_sec > (time_t)INT64_C(910692730085)) { errno = EINVAL; return -1; }
		ticks.QuadPart = (uint64_t)(times[i].tv_sec + INT64_C(11644473600)) * UINT64_C(10000000) + (uint64_t)times[i].tv_nsec / 100;
		values[i].dwLowDateTime = ticks.LowPart; values[i].dwHighDateTime = ticks.HighPart;
	}
	return SetFileTime(h, NULL, &values[0], &values[1]) ? 0 : pc_windows_errno(GetLastError());
}

int
pc_futimes(int fd, const struct timeval times[2])
{
	struct timespec ts[2];
	if (times == NULL) return pc_futimens(fd, NULL);
	for (int i = 0; i < 2; i++) {
		if (times[i].tv_usec < 0 || times[i].tv_usec >= 1000000L) { errno = EINVAL; return -1; }
		ts[i].tv_sec = (time_t)times[i].tv_sec; ts[i].tv_nsec = times[i].tv_usec * 1000;
	}
	return pc_futimens(fd, ts);
}

/* Reopen the same object for private positional I/O, never seek a shared fd. */
ssize_t
pc_pread(int fd, void *buffer, size_t size, pc_off_t offset)
{
	HANDLE original = (HANDLE)_get_osfhandle(fd), reader;
	OVERLAPPED operation;
	DWORD got = 0, error;
	char byte;
	int result = -1;

	if (original == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
	if (offset < 0) { errno = EINVAL; return -1; }
	if (size != 0 && buffer == NULL) { errno = EFAULT; return -1; }
	if (GetFileType(original) != FILE_TYPE_DISK) { errno = ESPIPE; return -1; }
	/* ReOpenFile may request additional rights; require the supplied handle
	 * itself to be readable before opening the private I/O handle. */
	if (!ReadFile(original, &byte, 0, &got, NULL) &&
	    GetLastError() != ERROR_HANDLE_EOF)
		return pc_windows_errno(GetLastError());
	if (size == 0) return 0;
	reader = ReOpenFile(original, GENERIC_READ,
	    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	    FILE_FLAG_OVERLAPPED | FILE_FLAG_OPEN_REPARSE_POINT);
	if (reader == INVALID_HANDLE_VALUE) return pc_windows_errno(GetLastError());
	memset(&operation, 0, sizeof(operation));
	operation.Offset = (DWORD)((uint64_t)offset & UINT32_MAX);
	operation.OffsetHigh = (DWORD)((uint64_t)offset >> 32);
	operation.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
	if (operation.hEvent == NULL) {
		pc_windows_errno(GetLastError()); CloseHandle(reader); return -1;
	}
	if (!ReadFile(reader, buffer, (DWORD)(size > INT_MAX ? INT_MAX : size),
	    NULL, &operation)) {
		error = GetLastError();
		if (error == ERROR_HANDLE_EOF) { got = 0; result = 0; }
		else if (error != ERROR_IO_PENDING) pc_windows_errno(error);
		else if (GetOverlappedResult(reader, &operation, &got, TRUE)) result = 0;
		else {
			error = GetLastError();
			if (error == ERROR_HANDLE_EOF) { got = 0; result = 0; }
			else pc_windows_errno(error);
		}
	} else if (GetOverlappedResult(reader, &operation, &got, TRUE)) result = 0;
	else pc_windows_errno(GetLastError());
	CloseHandle(operation.hEvent); CloseHandle(reader);
	return result == 0 ? (ssize_t)got : -1;
}

pc_off_t pc_lseek(int fd, pc_off_t offset, int origin) { return _lseeki64(fd, offset, origin); }
ssize_t pc_read(int fd, void *buffer, size_t size) { return (ssize_t)_read(fd, buffer, (unsigned int)(size > INT_MAX ? INT_MAX : size)); }
ssize_t pc_write(int fd, const void *buffer, size_t size) { return (ssize_t)_write(fd, buffer, (unsigned int)(size > INT_MAX ? INT_MAX : size)); }
int pc_fsync(int fd) { return _commit(fd); }
int pc_ftruncate(int fd, pc_off_t size) { int r = _chsize_s(fd, size); if (r) { errno = r; return -1; } return 0; }
unsigned int pc_sleep(unsigned int seconds) { while (seconds > 4000) { Sleep(4000000); seconds -= 4000; } Sleep(seconds * 1000); return 0; }

struct pc_directory { HANDLE find; WIN32_FIND_DATAW data; int first; struct dirent entry; };
DIR *
opendir(const char *path)
{
	wchar_t *wide = pc_windows_path(path), *pattern;
	DIR *dir;
	size_t len;
	DWORD attrs;
	if (wide == NULL) return NULL;
	if (check_components(wide) == -1) { free(wide); return NULL; }
	if (check_named(wide) == -1) { free(wide); return NULL; }
	attrs = GetFileAttributesW(wide);
	if (attrs == INVALID_FILE_ATTRIBUTES) { pc_windows_errno(GetLastError()); free(wide); return NULL; }
	if ((attrs & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != FILE_ATTRIBUTE_DIRECTORY) { free(wide); errno = ENOTDIR; return NULL; }
	len = wcslen(wide); pattern = malloc((len + 3) * sizeof(*pattern));
	if (pattern == NULL) { free(wide); return NULL; }
	memcpy(pattern, wide, len * sizeof(*wide)); pattern[len] = L'\\'; pattern[len + 1] = L'*'; pattern[len + 2] = 0; free(wide);
	dir = calloc(1, sizeof(*dir));
	if (dir == NULL) { free(pattern); return NULL; }
	dir->find = FindFirstFileW(pattern, &dir->data); free(pattern);
	if (dir->find == INVALID_HANDLE_VALUE) { pc_windows_errno(GetLastError()); free(dir); return NULL; }
	dir->first = 1; return dir;
}

struct dirent *
readdir(DIR *dir)
{
	char *utf8;
	if (dir == NULL) { errno = EBADF; return NULL; }
	if (!dir->first && !FindNextFileW(dir->find, &dir->data)) {
		DWORD error = GetLastError();
		if (error == ERROR_NO_MORE_FILES) errno = 0; else pc_windows_errno(error);
		return NULL;
	}
	dir->first = 0;
	utf8 = pc_windows_utf8(dir->data.cFileName);
	if (utf8 == NULL) return NULL;
	if (strlen(utf8) >= sizeof(dir->entry.d_name)) { free(utf8); errno = ENAMETOOLONG; return NULL; }
	memcpy(dir->entry.d_name, utf8, strlen(utf8) + 1); free(utf8);
	dir->entry.d_fileno = 1; /* Nonzero: Win32 enumeration does not supply inode IDs. */
	dir->entry.d_type = (dir->data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? DT_LNK :
	    ((dir->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? DT_DIR : DT_REG);
	return &dir->entry;
}
int closedir(DIR *dir) { int result; if (dir == NULL) { errno = EBADF; return -1; } result = FindClose(dir->find) ? 0 : pc_windows_errno(GetLastError()); free(dir); return result; }
