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
/* Native Windows account, environment, time and readiness. ISC license. */
#include "windows_compat.h"
#include <pwd.h>
#include <poll.h>
#include <sys/resource.h>
#include <wchar.h>
#include <limits.h>

struct environment_value { char *name; char *value; struct environment_value *next; };
static struct environment_value *environment_values;
static int environment_cleanup_registered;
static HANDLE memory_job;

static void
free_environment(void)
{
	struct environment_value *entry, *next;
	for (entry = environment_values; entry != NULL; entry = next) {
		next = entry->next; free(entry->name); free(entry->value); free(entry);
	}
}

char *
pc_getenv(const char *name)
{
	wchar_t *wide_name = pc_windows_utf16(name), *wide_value;
	struct environment_value *entry;
	char *utf8;
	DWORD len, got, error;
	if (wide_name == NULL) return NULL;
	SetLastError(ERROR_SUCCESS);
	len = GetEnvironmentVariableW(wide_name, NULL, 0);
	if (len == 0 && GetLastError() != ERROR_SUCCESS) {
		free(wide_name); return NULL;
	}
	wide_value = calloc((size_t)len + 1, sizeof(*wide_value));
	if (wide_value == NULL) { free(wide_name); return NULL; }
	SetLastError(ERROR_SUCCESS);
	got = GetEnvironmentVariableW(wide_name, wide_value, len + 1);
	error = GetLastError();
	free(wide_name);
	if (got > len || (got == 0 && error != ERROR_SUCCESS)) {
		if (got > len) errno = EAGAIN; else pc_windows_errno(error);
		free(wide_value); return NULL;
	}
	utf8 = pc_windows_utf8(wide_value); free(wide_value);
	if (utf8 == NULL) return NULL;
	for (entry = environment_values; entry != NULL; entry = entry->next)
		if (!_stricmp(entry->name, name)) break;
	if (entry == NULL) {
		entry = calloc(1, sizeof(*entry));
		if (entry == NULL) { free(utf8); return NULL; }
		entry->name = _strdup(name);
		if (entry->name == NULL) { free(entry); free(utf8); return NULL; }
		entry->next = environment_values; environment_values = entry;
	}
	if (entry->value != NULL && !strcmp(entry->value, utf8)) {
		free(utf8); return entry->value;
	}
	free(entry->value); entry->value = utf8;
	if (!environment_cleanup_registered) {
		if (atexit(free_environment) != 0) { errno = ENOMEM; return NULL; }
		environment_cleanup_registered = 1;
	}
	return entry->value;
}

int
pc_setenv(const char *name, const char *value, int overwrite)
{
	wchar_t *key, *wide;
	DWORD exists;
	int result;
	if (name == NULL || !*name || strchr(name, '=') || value == NULL) { errno = EINVAL; return -1; }
	key = pc_windows_utf16(name); if (key == NULL) return -1;
	if (!overwrite) {
		SetLastError(ERROR_SUCCESS); exists = GetEnvironmentVariableW(key, NULL, 0);
		if (exists != 0 || GetLastError() == ERROR_SUCCESS) { free(key); return 0; }
	}
	wide = pc_windows_utf16(value); if (wide == NULL) { free(key); return -1; }
	result = _wputenv_s(key, wide);
	if (result != 0) { errno = result; result = -1; }
	else result = SetEnvironmentVariableW(key, wide) ? 0 : pc_windows_errno(GetLastError());
	free(wide); free(key); return result;
}

int
pc_unsetenv(const char *name)
{
	wchar_t *key;
	int result;
	if (name == NULL || !*name || strchr(name, '=')) { errno = EINVAL; return -1; }
	key = pc_windows_utf16(name); if (key == NULL) return -1;
	result = _wputenv_s(key, L"");
	if (result != 0) { errno = result; result = -1; }
	else if (!SetEnvironmentVariableW(key, NULL) && GetLastError() != ERROR_ENVVAR_NOT_FOUND)
		result = pc_windows_errno(GetLastError());
	free(key); return result;
}

static uint32_t
sid_identity(PSID sid)
{
	const unsigned char *bytes = sid;
	DWORD i, count = GetLengthSid(sid);
	uint32_t hash = UINT32_C(2166136261);
	for (i = 0; i < count; i++) hash = (hash ^ bytes[i]) * UINT32_C(16777619);
	return hash;
}

uid_t
pc_getuid(void)
{
	HANDLE token;
	TOKEN_USER *user;
	DWORD bytes = 0;
	uid_t value;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
		pc_windows_errno(GetLastError()); return UINT32_MAX;
	}
	(void)GetTokenInformation(token, TokenUser, NULL, 0, &bytes);
	user = malloc(bytes);
	if (user == NULL) { CloseHandle(token); return UINT32_MAX; }
	if (!GetTokenInformation(token, TokenUser, user, bytes, &bytes)) {
		pc_windows_errno(GetLastError()); free(user); CloseHandle(token); return UINT32_MAX;
	}
	value = sid_identity(user->User.Sid);
	free(user); CloseHandle(token); return value;
}

struct passwd *
getpwuid(uid_t uid)
{
	static struct passwd account;
	static char *name;
	DWORD len = 0;
	wchar_t *wide;
	uid_t current = pc_getuid();
	if (current == UINT32_MAX || uid != current) { errno = ENOTSUP; return NULL; }
	if (name == NULL) {
		(void)GetUserNameW(NULL, &len);
		if (len == 0) { pc_windows_errno(GetLastError()); return NULL; }
		wide = malloc((size_t)len * sizeof(*wide));
		if (wide == NULL) return NULL;
		if (!GetUserNameW(wide, &len)) { pc_windows_errno(GetLastError()); free(wide); return NULL; }
		name = pc_windows_utf8(wide); free(wide);
		if (name == NULL) return NULL;
	}
	account.pw_name = name; account.pw_uid = current; account.pw_gid = UINT32_MAX;
	account.pw_passwd = "*"; account.pw_gecos = name;
	account.pw_dir = pc_getenv("USERPROFILE");
	if (account.pw_dir == NULL) { errno = ENOENT; return NULL; }
	account.pw_shell = ""; /* No shell execution is implied by native account data. */
	return &account;
}

struct passwd *
getpwnam(const char *name)
{
	struct passwd *account = getpwuid(pc_getuid());
	if (account != NULL && !_stricmp(account->pw_name, name)) return account;
	errno = ENOTSUP; return NULL;
}

int
pc_gethostname(char *out, size_t size)
{
	DWORD len = 0;
	wchar_t *wide;
	char *utf8;
	(void)GetComputerNameExW(ComputerNameDnsHostname, NULL, &len);
	if (len == 0) return pc_windows_errno(GetLastError());
	wide = malloc((size_t)len * sizeof(*wide));
	if (wide == NULL) return -1;
	if (!GetComputerNameExW(ComputerNameDnsHostname, wide, &len)) { pc_windows_errno(GetLastError()); free(wide); return -1; }
	utf8 = pc_windows_utf8(wide); free(wide);
	if (utf8 == NULL) return -1;
	if (strlen(utf8) >= size) { free(utf8); errno = ENAMETOOLONG; return -1; }
	memcpy(out, utf8, strlen(utf8) + 1); free(utf8); return 0;
}

/* Gregorian calendar arithmetic also handles valid pre-1970 RCS dates. */
static int64_t
civil_days(int64_t year, int month, int day)
{
	int64_t era, yoe, doy, doe;
	year -= month <= 2;
	era = (year >= 0 ? year : year - 399) / 400;
	yoe = year - era * 400;
	doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + doe - 719468;
}

struct tm *
pc_gmtime_r(const time_t *value, struct tm *out)
{
	int64_t days, rem, z, era, doe, yoe, year, doy, mp, month, day, weekday;
	if (value == NULL || out == NULL) { errno = EINVAL; return NULL; }
	days = (int64_t)*value / 86400; rem = (int64_t)*value % 86400;
	if (rem < 0) { rem += 86400; days--; }
	z = days + 719468;
	era = (z >= 0 ? z : z - 146096) / 146097;
	doe = z - era * 146097;
	yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	year = yoe + era * 400;
	doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	mp = (5 * doy + 2) / 153;
	day = doy - (153 * mp + 2) / 5 + 1;
	month = mp + (mp < 10 ? 3 : -9);
	year += month <= 2;
	if (year - 1900 < INT_MIN || year - 1900 > INT_MAX) { errno = EOVERFLOW; return NULL; }
	memset(out, 0, sizeof(*out));
	out->tm_year = (int)(year - 1900); out->tm_mon = (int)month - 1; out->tm_mday = (int)day;
	out->tm_hour = (int)(rem / 3600); out->tm_min = (int)(rem / 60 % 60); out->tm_sec = (int)(rem % 60);
	weekday = (days + 4) % 7; if (weekday < 0) weekday += 7;
	out->tm_wday = (int)weekday; out->tm_yday = (int)(days - civil_days(year, 1, 1));
	out->tm_isdst = 0; return out;
}

time_t
pc_timegm(struct tm *value)
{
	int64_t year, month, seconds;
	time_t result;
	if (value == NULL) { errno = EINVAL; return (time_t)-1; }
	year = (int64_t)value->tm_year + 1900; month = value->tm_mon;
	year += month / 12; month %= 12;
	if (month < 0) { month += 12; year--; }
	seconds = (civil_days(year, (int)month + 1, 1) + (int64_t)value->tm_mday - 1) * 86400;
	seconds += (int64_t)value->tm_hour * 3600 + (int64_t)value->tm_min * 60 + value->tm_sec;
	result = (time_t)seconds;
	if ((int64_t)result != seconds || pc_gmtime_r(&result, value) == NULL) { errno = EOVERFLOW; return (time_t)-1; }
	return result;
}

struct tm *
pc_localtime_r(const time_t *value, struct tm *out)
{
	DYNAMIC_TIME_ZONE_INFORMATION zone;
	TIME_ZONE_INFORMATION year_zone;
	SYSTEMTIME utc, local;
	struct tm normalized;
	time_t wall;
	int64_t offset;
	if (value == NULL || out == NULL) { errno = EINVAL; return NULL; }
	if (*value >= 0 && localtime_s(out, value) == 0) return out;
	if (pc_gmtime_r(value, &normalized) == NULL || normalized.tm_year < -299 || normalized.tm_year > 28927) { errno = EOVERFLOW; return NULL; }
	memset(&utc, 0, sizeof(utc)); utc.wYear = (WORD)(normalized.tm_year + 1900);
	utc.wMonth = (WORD)(normalized.tm_mon + 1); utc.wDay = (WORD)normalized.tm_mday;
	utc.wHour = (WORD)normalized.tm_hour; utc.wMinute = (WORD)normalized.tm_min; utc.wSecond = (WORD)normalized.tm_sec;
	if (GetDynamicTimeZoneInformation(&zone) == TIME_ZONE_ID_INVALID ||
	    !SystemTimeToTzSpecificLocalTimeEx(&zone, &utc, &local)) { pc_windows_errno(GetLastError()); return NULL; }
	memset(out, 0, sizeof(*out)); out->tm_year = local.wYear - 1900; out->tm_mon = local.wMonth - 1;
	out->tm_mday = local.wDay; out->tm_hour = local.wHour; out->tm_min = local.wMinute; out->tm_sec = local.wSecond;
	wall = pc_timegm(out); offset = (int64_t)wall - (int64_t)*value;
	if (GetTimeZoneInformationForYear(local.wYear, &zone, &year_zone) && year_zone.DaylightDate.wMonth != 0)
		out->tm_isdst = offset == -60 * (int64_t)(year_zone.Bias + year_zone.DaylightBias);
	return out;
}

char *
pc_asctime_r(const struct tm *t, char *out)
{
	static const char *const days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
	static const char *const months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	if (t == NULL || out == NULL || t->tm_wday < 0 || t->tm_wday > 6 ||
	    t->tm_mon < 0 || t->tm_mon > 11 || t->tm_mday < 1 || t->tm_mday > 31 ||
	    t->tm_hour < 0 || t->tm_hour > 23 || t->tm_min < 0 || t->tm_min > 59 ||
	    t->tm_sec < 0 || t->tm_sec > 60) { errno = EINVAL; return NULL; }
	if (t->tm_year < -1900 || t->tm_year > 8099) { errno = EOVERFLOW; return NULL; }
	if (snprintf(out, 26, "%s %s %2d %02d:%02d:%02d %04d\n", days[t->tm_wday], months[t->tm_mon],
	    t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, t->tm_year + 1900) != 25) { errno = EOVERFLOW; return NULL; }
	return out;
}

char *
pc_ctime_r(const time_t *value, char *out)
{
	struct tm t;
	if (pc_localtime_r(value, &t) == NULL) return NULL;
	return pc_asctime_r(&t, out);
}

int
pc_poll(struct pollfd *fds, unsigned long count, int timeout)
{
	ULONGLONG start = GetTickCount64();
	int ready;
	unsigned long i;
	DWORD type, available;
	HANDLE handle;
	if (timeout < -1 || (count != 0 && fds == NULL)) { errno = EINVAL; return -1; }
	for (;;) {
		ready = 0;
		for (i = 0; i < count; i++) {
			fds[i].revents = 0;
			if (fds[i].fd < 0) continue;
			handle = (HANDLE)_get_osfhandle(fds[i].fd);
			if (handle == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
			type = GetFileType(handle);
			if (type == FILE_TYPE_DISK) fds[i].revents = fds[i].events & (POLLIN | POLLOUT);
			else if (type == FILE_TYPE_PIPE && (fds[i].events & POLLIN)) {
				if (!PeekNamedPipe(handle, NULL, 0, NULL, &available, NULL)) {
					if (GetLastError() == ERROR_BROKEN_PIPE) fds[i].revents = POLLIN;
					else return pc_windows_errno(GetLastError());
				} else if (available > 0) fds[i].revents = POLLIN;
			} else { errno = ENOTSUP; return -1; }
			if (fds[i].revents) ready++;
		}
		if (ready || timeout == 0 || (timeout > 0 && GetTickCount64() - start >= (ULONGLONG)timeout)) return ready;
		Sleep(10);
	}
}

int
getrlimit(int resource, struct rlimit *limit)
{
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
	if (resource != RLIMIT_DATA || limit == NULL) { errno = EINVAL; return -1; }
	limit->rlim_cur = limit->rlim_max = RLIM_INFINITY;
	if (memory_job != NULL) {
		if (!QueryInformationJobObject(memory_job, JobObjectExtendedLimitInformation, &info, (DWORD)sizeof(info), NULL)) return pc_windows_errno(GetLastError());
		if (info.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_PROCESS_MEMORY)
			limit->rlim_cur = info.ProcessMemoryLimit;
	}
	return 0;
}

int
setrlimit(int resource, const struct rlimit *limit)
{
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
	HANDLE job;
	if (resource != RLIMIT_DATA || limit == NULL || limit->rlim_cur > limit->rlim_max) { errno = EINVAL; return -1; }
	if (limit->rlim_cur != RLIM_INFINITY && limit->rlim_cur > SIZE_MAX) { errno = EOVERFLOW; return -1; }
	memset(&info, 0, sizeof(info));
	if (limit->rlim_cur != RLIM_INFINITY) {
		info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
		info.ProcessMemoryLimit = (SIZE_T)limit->rlim_cur;
	}
	job = memory_job;
	if (job == NULL) {
		job = CreateJobObjectW(NULL, NULL);
		if (job == NULL) return pc_windows_errno(GetLastError());
	}
	if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, (DWORD)sizeof(info)) ||
	    (memory_job == NULL && !AssignProcessToJobObject(job, GetCurrentProcess()))) {
		DWORD error = GetLastError(); if (memory_job == NULL) CloseHandle(job); return pc_windows_errno(error);
	}
	memory_job = job; return 0;
}
