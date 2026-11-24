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
/* Portable OpenCVS native Windows interfaces. ISC license; see LICENSE. */
#ifndef OPENCVS_WINDOWS_COMPAT_H
#define OPENCVS_WINDOWS_COMPAT_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000004 /* Windows 10, version 1709. */
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include <direct.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#ifndef ENOTSUP
#define ENOTSUP EINVAL
#endif
#ifndef ELOOP
#define ELOOP EINVAL
#endif
#ifndef EOVERFLOW
#define EOVERFLOW ERANGE
#endif
#ifndef EILSEQ
#define EILSEQ EINVAL
#endif
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif
#ifndef _UID_T_DEFINED
#define _UID_T_DEFINED
typedef uint32_t uid_t;
#endif
#ifndef _GID_T_DEFINED
#define _GID_T_DEFINED
typedef uint32_t gid_t;
#endif
#ifdef _MSC_VER
#ifndef _MODE_T_DEFINED
#define _MODE_T_DEFINED
typedef unsigned short mode_t;
#endif
#ifndef _PID_T_DEFINED
#define _PID_T_DEFINED
typedef int pid_t;
#endif
#endif
#ifndef PC_HAVE_STRUCT_TIMESPEC
struct timespec {
	time_t tv_sec;
	long   tv_nsec;
};
#endif
#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval {
	long tv_sec;
	long tv_usec;
};
#endif
typedef unsigned char u_char;
typedef unsigned int  u_int;
typedef unsigned long u_long;
typedef uint16_t      u_int16_t;
typedef uint32_t      u_int32_t;
typedef uint64_t      u_int64_t;
typedef int64_t	      pc_off_t;
struct pc_timeval {
	time_t tv_sec;
	long   tv_usec;
};
struct pc_stat {
	uint64_t	st_dev, st_ino, st_nlink;
	mode_t		st_mode;
	uid_t		st_uid;
	gid_t		st_gid;
	uint64_t	st_rdev;
	pc_off_t	st_size;
	long		st_blksize;
	struct timespec st_atim, st_mtim, st_ctim;
};
#ifndef S_IFMT
#define S_IFMT 0170000
#endif
#ifndef S_IFDIR
#define S_IFDIR 0040000
#endif
#ifndef S_IFREG
#define S_IFREG 0100000
#endif
#ifndef S_IFCHR
#define S_IFCHR 0020000
#endif
#ifndef S_IFBLK
#define S_IFBLK 0060000
#endif
#ifndef S_IFLNK
#define S_IFLNK 0120000
#endif
#ifndef S_IFIFO
#define S_IFIFO 0010000
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_ISLNK
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#endif
#ifndef S_IRUSR
#define S_IRUSR 0400
#define S_IWUSR 0200
#define S_IXUSR 0100
#define S_IRGRP 0040
#define S_IWGRP 0020
#define S_IXGRP 0010
#define S_IROTH 0004
#define S_IWOTH 0002
#define S_IXOTH 0001
#define S_IRWXU 0700
#define S_IRWXG 0070
#define S_IRWXO 0007
#endif
#ifndef O_BINARY
#define O_BINARY _O_BINARY
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0x10000000
#endif
#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#else
#undef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef MAXPATHLEN
#define MAXPATHLEN PATH_MAX
#endif
#ifndef NAME_MAX
#define NAME_MAX 255
#endif
#ifndef MAXNAMLEN
#define MAXNAMLEN NAME_MAX
#endif
/* POSIX minimum required by the imported BSD regular-expression parser. */
#ifndef _POSIX2_RE_DUP_MAX
#define _POSIX2_RE_DUP_MAX 255
#endif
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif
#ifndef R_OK
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0
#endif
wchar_t *pc_windows_utf16(const char *);
char	*pc_windows_utf8(const wchar_t *);
wchar_t *pc_windows_path(const char *);
int	 pc_windows_errno(DWORD);
void	 pc_windows_startup(int *, char ***);
char	*pc_windows_tempdir(void);
#ifdef _MSC_VER
char *pc_strerror(int);
#endif
char	    *pc_strptime(const char *, const char *, struct tm *);
int	     pc_open(const char *, int, ...);
FILE	    *pc_fopen(const char *, const char *);
FILE	    *pc_fdopen(int, const char *);
int	     pc_stat(const char *, struct pc_stat *);
int	     pc_lstat(const char *, struct pc_stat *);
int	     pc_fstat(int, struct pc_stat *);
int	     pc_unlink(const char *);
int	     pc_rename(const char *, const char *);
const char  *pc_windows_rename_error_stage(void);
int	     pc_mkdir(const char *, mode_t);
int	     pc_rmdir(const char *);
int	     pc_chdir(const char *);
char	    *pc_getcwd(char *, size_t);
int	     pc_chmod(const char *, mode_t);
int	     pc_fchmod(int, mode_t);
int	     pc_access(const char *, int);
mode_t	     pc_umask(mode_t);
int	     pc_mkstemp(char *);
char	    *pc_mkdtemp(char *);
int	     pc_futimes(int, const struct pc_timeval[2]);
int	     pc_futimens(int, const struct timespec[2]);
pc_off_t     pc_lseek(int, pc_off_t, int);
ssize_t	     pc_pread(int, void *, size_t, pc_off_t);
ssize_t	     pc_read(int, void *, size_t);
ssize_t	     pc_write(int, const void *, size_t);
int	     pc_fsync(int);
int	     pc_ftruncate(int, pc_off_t);
unsigned int pc_sleep(unsigned int);
int	     pc_gethostname(char *, size_t);
uid_t	     pc_getuid(void);
char	    *pc_getenv(const char *);
int	     pc_setenv(const char *, const char *, int);
int	     pc_unsetenv(const char *);
struct tm   *pc_gmtime_r(const time_t *, struct tm *);
struct tm   *pc_localtime_r(const time_t *, struct tm *);
char	    *pc_ctime_r(const time_t *, char *);
char	    *pc_asctime_r(const struct tm *, char *);
time_t	     pc_timegm(struct tm *);
#ifndef OPENCVS_WINDOWS_NO_ALIASES
#define off_t pc_off_t
#define timeval pc_timeval
#define stat pc_stat
#define lstat pc_lstat
#define fstat pc_fstat
#define st_atime st_atim.tv_sec
#define st_mtime st_mtim.tv_sec
#define st_ctime st_ctim.tv_sec
#define open pc_open
#define fopen pc_fopen
#define fdopen pc_fdopen
#define unlink pc_unlink
#define rename pc_rename
#define mkdir pc_mkdir
#define rmdir pc_rmdir
#define chdir pc_chdir
#define getcwd pc_getcwd
#define chmod pc_chmod
#define fchmod pc_fchmod
#define access pc_access
#define mkstemp pc_mkstemp
#define mkdtemp pc_mkdtemp
#define futimes pc_futimes
#define futimens pc_futimens
#define lseek pc_lseek
#define pread pc_pread
#define read pc_read
#define write pc_write
#define getpid _getpid
#ifdef _MSC_VER
#define strerror pc_strerror
#endif
#define close _close
#define dup _dup
#define dup2 _dup2
#define fileno _fileno
#define isatty _isatty
#define umask pc_umask
#define fsync pc_fsync
#define ftruncate pc_ftruncate
#define sleep pc_sleep
#define gethostname pc_gethostname
#define getuid pc_getuid
#define geteuid pc_getuid
#define getenv pc_getenv
#define setenv pc_setenv
#define unsetenv pc_unsetenv
#define gmtime_r pc_gmtime_r
#define localtime_r pc_localtime_r
#define ctime_r pc_ctime_r
#define asctime_r pc_asctime_r
#define timegm pc_timegm
#define strptime pc_strptime
#define strdup _strdup
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define tzset _tzset
#endif
#endif
