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
/* Portable OpenCVS compatibility declarations. SPDX-License-Identifier: ISC */
#ifndef PC_PORTABLE_H
#define PC_PORTABLE_H
/* The Windows dirent shim consumes the feature configuration. */
#include "portable_config.h"

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include "windows_compat.h"
#else
#include <sys/types.h>

#include <unistd.h>
#endif
#ifdef _MSC_VER
#define PC_NORETURN __declspec(noreturn)
#else
#define PC_NORETURN __attribute__((noreturn))
#endif
#if defined(__GNUC__) || defined(__clang__)
#if defined(__MINGW32__)
#define PC_PRINTF(a, b) __attribute__((format(gnu_printf, a, b)))
#else
#define PC_PRINTF(a, b) __attribute__((format(printf, a, b)))
#endif
#else
#define PC_PRINTF(a, b)
#endif
#if defined(_MSC_VER) && !defined(__clang__)
#define PC_UNUSED
#else
#define PC_UNUSED __attribute__((unused))
#endif
#if defined(_MSC_VER) && !defined(__clang__)
#define __attribute__(x)
#endif
#if defined(__has_attribute)
#if __has_attribute(__bounded__)
#define PC_BOUNDED_STRING(a, b) __attribute__((__bounded__(__string__, a, b)))
#endif
#endif
#ifndef PC_BOUNDED_STRING
#define PC_BOUNDED_STRING(a, b)
#endif
#ifndef __BEGIN_DECLS
#define __BEGIN_DECLS
#define __END_DECLS
#endif
#ifndef DEF_WEAK
/* libc symbol interposition metadata is not needed by this standalone target.
 */
#define DEF_WEAK(name) typedef int pc_weak_marker_##name
#endif
#ifndef _DIAGASSERT
#define _DIAGASSERT(expr) assert(expr)
#endif
#ifndef PC_HAVE_FNM_CASEFOLD
#define fnmatch pc_fnmatch
#endif
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#ifndef MAXNAMLEN
/* Pattern storage capacity in bytes, not an asserted filesystem limit. */
#define MAXNAMLEN 4096
#endif
#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif
#ifndef ACCESSPERMS
#define ACCESSPERMS 0777
#endif
#ifndef ALLPERMS
#define ALLPERMS 07777
#endif
#ifndef DEFFILEMODE
#define DEFFILEMODE 0666
#endif
#ifndef WAIT_ANY
#define WAIT_ANY (-1)
#endif
#ifndef PC_HAVE_STRLCPY
size_t strlcpy(char *, const char *, size_t);
#endif
#ifndef PC_HAVE_STRLCAT
size_t strlcat(char *, const char *, size_t);
#endif
#ifndef PC_HAVE_STRTONUM
long long strtonum(const char *, long long, long long, const char **);
#endif
#ifndef PC_HAVE_REALLOCARRAY
void *reallocarray(void *, size_t, size_t);
#endif
#ifndef PC_HAVE_STRSEP
char *strsep(char **, const char *);
#endif
/* Namespace fallbacks when a native declaration is unavailable. */
#ifndef PC_HAVE_ASPRINTF
#define asprintf pc_asprintf
#endif
#ifndef PC_HAVE_VASPRINTF
#define vasprintf pc_vasprintf
#endif
int   asprintf(char **, const char *, ...) PC_PRINTF(2, 3);
int   vasprintf(char **, const char *, va_list) PC_PRINTF(2, 0);
char *pc_fgetln(FILE *, size_t *);
char *pc_metadata_getln(FILE *, size_t *);
char *pc_metadata_fgets(char *, int, FILE *);
int   pc_fclose(FILE *);
void  pc_set_metadata_error_handler(void (*)(const char *));
DIR  *pc_directory_open(int, const char *);
/* 0 synced, 1 unsupported, -1 failed with errno. */
int pc_sync_parent_directory(const char *);
#ifndef PC_HAVE_FGETLN
#define fgetln pc_fgetln
#endif
#define fclose pc_fclose
extern char *pc_progname;
#define __progname pc_progname
const char *pc_getprogname(void);
void	    pc_platform_startup(int *, char ***);
char	   *pc_default_tmpdir(void);
int	    pc_native_absolute(const char *);
int	    pc_any_absolute(const char *);
void	    pc_strip_slash(char *);
/* Namespaced BSD getopt preserves optreset on every supported libc. */
#ifndef PC_HAVE_BSD_GETOPT
int	     pc_getopt(int, char *const *, const char *);
extern char *pc_optarg;
extern int   pc_opterr, pc_optind, pc_optopt, pc_optreset;
#define getopt pc_getopt
#define optarg pc_optarg
#define opterr pc_opterr
#define optind pc_optind
#define optopt pc_optopt
#define optreset pc_optreset
#define getopt_long pc_getopt_long
#define getopt_long_only pc_getopt_long_only
#endif
#endif
