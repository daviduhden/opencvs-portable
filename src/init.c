/*
 * Portable OpenCVS changes only; original notices below remain in force.
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
/*	$OpenBSD: init.c,v 1.42 2021/10/25 10:07:12 deraadt Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2006 Xavier Santolaria <xsa@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "atomicio.h"
#include "cvs.h"
#include "init.h"
#include "remote.h"

void	cvs_init_local(void);

static void init_mkdir(const char *, mode_t);
static void init_mkfile(char *, char **);

struct cvsroot_file {
	char			*cf_path;
	char			**cf_content;
};

static const struct cvsroot_file cvsroot_files[] = {
	{ CVS_PATH_CHECKOUTLIST,	NULL			},
	{ CVS_PATH_COMMITINFO,		NULL			},
	{ CVS_PATH_CONFIG,		config_contents		},
	{ CVS_PATH_CVSWRAPPERS,		NULL			},
	{ CVS_PATH_EDITINFO,		NULL			},
	{ CVS_PATH_HISTORY,		NULL			},
	{ CVS_PATH_LOGINFO,		NULL			},
	{ CVS_PATH_MODULES,		NULL			},
	{ CVS_PATH_NOTIFY_R,		NULL			},
	{ CVS_PATH_RCSINFO,		NULL			},
	{ CVS_PATH_TAGINFO,		NULL			},
	{ CVS_PATH_VALTAGS,		NULL			},
	{ CVS_PATH_VERIFYMSG,		NULL			}
};

static const char *cvsroot_dirs[2] = {
	CVS_PATH_ROOT, CVS_PATH_EMPTYDIR
};

#define INIT_NFILES	(sizeof(cvsroot_files)/sizeof(cvsroot_files[0]))
#define INIT_NDIRS	(sizeof(cvsroot_dirs)/sizeof(cvsroot_dirs[0]))

struct cvs_cmd cvs_cmd_init = {
	CVS_OP_INIT, 0, "init",
	{ { 0 }, { 0 } },
	"Create a CVS repository if it doesn't exist",
	"",
	"",
	NULL,
	cvs_init
};

int
cvs_init(int argc, char **argv)
{
	(void)argv;
	if (argc > 1)
		fatal("init does not take any extra arguments");

	if (cvsroot_is_remote()) {
		cvs_client_connect_to_server();
		cvs_client_send_request("init %s", current_cvsroot->cr_dir);
		cvs_client_get_responses();
	} else {
		cvs_init_local();
	}

	return (0);
}

void
cvs_init_local(void)
{
	u_int i;
	char path[PATH_MAX];

	cvs_log(LP_TRACE, "cvs_init_local()");

	/* Create repository root directory if it does not already exist */
	init_mkdir(current_cvsroot->cr_dir, 0777);

	for (i = 0; i < INIT_NDIRS; i++) {
		(void)xsnprintf(path, PATH_MAX, "%s/%s",
		    current_cvsroot->cr_dir, cvsroot_dirs[i]);

		init_mkdir(path, 0777);
	}

	for (i = 0; i < INIT_NFILES; i++) {
		(void)xsnprintf(path, PATH_MAX, "%s/%s",
		    current_cvsroot->cr_dir, cvsroot_files[i].cf_path);

		init_mkfile(path, cvsroot_files[i].cf_content);
	}
}

static void
init_mkdir(const char *path, mode_t mode)
{
	struct stat st;

	if (mkdir(path, mode) == -1) {
		if ((errno != EEXIST && errno != EACCES) ||
		    lstat(path, &st) == -1 || !S_ISDIR(st.st_mode))
			fatal("init_mkdir: `%s' is not an accessible directory", path);
	}
}

static void
init_mkfile(char *path, char **content)
{
	BUF *b;
	size_t len;
	int fd, rcsflags;
	char rpath[PATH_MAX];
	char **p;
	RCSFILE *file;
	struct stat existing;

	rcsflags = RCS_WRITE | RCS_CREATE;
	/* Repeated init preserves every existing administrator file and archive. */
	if (lstat(path, &existing) == 0) {
		if (!S_ISREG(existing.st_mode))
			fatal("init_mkfile: existing path is not a regular file: %s", path);
		return;
	}
	if (errno != ENOENT)
		fatal("init_mkfile: inspect %s: %s", path, strerror(errno));
	(void)xsnprintf(rpath, PATH_MAX, "%s%s", path, RCS_FILE_EXT);
	if (lstat(rpath, &existing) == 0)
		fatal("init_mkfile: archive exists without administrator file: %s", rpath);
	if (errno != ENOENT)
		fatal("init_mkfile: inspect %s: %s", rpath, strerror(errno));

	if ((fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0444)) == -1)
		fatal("init_mkfile: open: `%s': %s", path, strerror(errno));

	if (content != NULL) {
		for (p = content; *p != NULL; ++p) {
			len = strlen(*p);
			if (atomicio(vwrite, fd, *p, len) != len)
				fatal("init_mkfile: atomicio failed");
		}
	}

	/*
	 * Make sure history and val-tags files are world-writable.
	 * Every user should be able to write to them.
	 */
	if (strcmp(strrchr(CVS_PATH_HISTORY, '/'), strrchr(path, '/')) == 0 ||
	    strcmp(strrchr(CVS_PATH_VALTAGS, '/'), strrchr(path, '/')) == 0) {
		if (fchmod(fd, 0666) == -1 || fsync(fd) == -1 || close(fd) == -1)
			fatal("init_mkfile: finish %s: %s", path, strerror(errno));
		return;
	}

	(void)xsnprintf(rpath, PATH_MAX, "%s%s", path, RCS_FILE_EXT);

	if ((file = rcs_open(rpath, -1, rcsflags, 0444)) == NULL)
		fatal("failed to create RCS file for `%s'", path);

	b = buf_load(path);

	if (rcs_rev_add(file, RCS_HEAD_REV, "initial checkin", -1, NULL) == -1)
		fatal("init_mkfile: failed to add new revision");

	/* b buffer is free'd in rcs_deltatext_set */
	if (rcs_deltatext_set(file, file->rf_head, b) == -1)
		fatal("init_mkfile: failed to set delta");

	file->rf_flags &= ~RCS_SYNCED;
	rcs_close(file);
	if (fsync(fd) == -1 || close(fd) == -1)
		fatal("init_mkfile: finish %s: %s", path, strerror(errno));
}
