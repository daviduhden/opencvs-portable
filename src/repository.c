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
/*	$OpenBSD: repository.c,v 1.26 2019/06/28 13:35:00 deraadt Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
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

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"

struct wklhead repo_locks;

void
cvs_repository_unlock(const char *repo)
{
	struct worklist *lock;
	char path[PATH_MAX];
	(void)xsnprintf(path, sizeof(path), "%s/%s", repo, CVS_LOCK);
	SLIST_FOREACH(lock, &repo_locks, wkl_list) {
		if (strcmp(lock->wkl_path, path) != 0)
			continue;
		SLIST_REMOVE(&repo_locks, lock, worklist, wkl_list);
		if (unlink(path) == -1 && errno != ENOENT)
			cvs_log(LP_ERR, "cannot remove owned repository lock: %s", path);
		free(lock);
		break;
	}
}

void
cvs_repository_lock(const char *repo, int wantlock)
{
	struct worklist *owned;
	struct stat st;
	char path[PATH_MAX];
	int attempt, fd;

	if (cvs_noexec || cvs_readonlyfs)
		return;
	(void)xsnprintf(path, sizeof(path), "%s/%s", repo, CVS_LOCK);
	SLIST_FOREACH(owned, &repo_locks, wkl_list)
		if (strcmp(owned->wkl_path, path) == 0)
			return;
	for (attempt = 0; attempt < CVS_LOCK_TRIES; attempt++) {
		if (cvs_quit)
			fatal("received signal %d", sig_received);
		if (wantlock) {
			fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
			if (fd != -1) {
				worklist_add(path, &repo_locks);
				if (close(fd) == -1)
					fatal("cannot close repository lock: %s", strerror(errno));
				return;
			}
			if (errno != EEXIST)
				fatal("cannot acquire repository lock %s: %s", path, strerror(errno));
		} else {
			if (lstat(path, &st) == -1) {
				if (errno == ENOENT)
					return;
				fatal("cannot inspect repository lock %s: %s", path, strerror(errno));
			}
		}
		cvs_log(LP_NOTICE, "waiting for repository lock in '%s'", repo);
		(void)sleep(CVS_LOCK_SLEEP);
	}
	fatal("maximum wait time for repository lock in '%s' reached", repo);
}

void
cvs_repository_getdir(const char *dir, const char *wdir,
	struct cvs_flisthead *fl, struct cvs_flisthead *dl, int flags)
{
	int type;
	DIR *dirp;
	struct stat st;
	struct dirent *dp;
	char *s, fpath[PATH_MAX], rpath[PATH_MAX];

	if ((dirp = opendir(dir)) == NULL)
		fatal("cvs_repository_getdir: failed to open '%s'", dir);

	while ((dp = readdir(dirp)) != NULL) {
		if (!strcmp(dp->d_name, ".") ||
		    !strcmp(dp->d_name, "..") ||
		    !strcmp(dp->d_name, CVS_LOCK))
			continue;

		(void)xsnprintf(fpath, PATH_MAX, "%s/%s", wdir, dp->d_name);
		(void)xsnprintf(rpath, PATH_MAX, "%s/%s", dir, dp->d_name);

		if (!TAILQ_EMPTY(&checkout_ign_pats)) {
			if ((s = strrchr(fpath, ',')) != NULL)
				*s = '\0';
			if (cvs_file_chkign(fpath))
				continue;
			if (s != NULL)
				*s = ',';
		}

		/*
		 * nfs and afs will show d_type as DT_UNKNOWN
		 * for files and/or directories so when we encounter
		 * this we call lstat() on the path to be sure.
		 */
		if (dp->d_type == DT_UNKNOWN) {
			if (lstat(rpath, &st) == -1)
				fatal("'%s': %s", rpath, strerror(errno));

			switch (st.st_mode & S_IFMT) {
			case S_IFDIR:
				type = CVS_DIR;
				break;
			case S_IFREG:
				type = CVS_FILE;
				break;
			default:
				fatal("Unknown file type in repository");
			}
		} else {
			switch (dp->d_type) {
			case DT_DIR:
				type = CVS_DIR;
				break;
			case DT_REG:
				type = CVS_FILE;
				break;
			default:
				fatal("Unknown file type in repository");
			}
		}

		if (!(flags & REPOSITORY_DODIRS) && type == CVS_DIR) {
			if (strcmp(dp->d_name, CVS_PATH_ATTIC))
				continue;
		}

		switch (type) {
		case CVS_DIR:
			if (!strcmp(dp->d_name, CVS_PATH_ATTIC)) {
				cvs_repository_getdir(rpath, wdir, fl, dl,
				    REPOSITORY_IS_ATTIC);
			} else {
				cvs_file_get(fpath, 0, dl, CVS_DIR);
			}
			break;
		case CVS_FILE:
			if ((s = strrchr(fpath, ',')) != NULL &&
			    s != fpath && !strcmp(s, RCS_FILE_EXT)) {
				*s = '\0';
				cvs_file_get(fpath, 
				    (flags & REPOSITORY_IS_ATTIC) ?
				    FILE_INSIDE_ATTIC : 0, fl, CVS_FILE);
			}
			break;
		default:
			fatal("type %d unknown, shouldn't happen", type);
		}
	}

	(void)closedir(dirp);
}
