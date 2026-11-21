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
/*	$OpenBSD: history.c,v 1.46 2024/05/21 05:00:48 jsg Exp $	*/
/*
 * Copyright (c) 2007 Joris Vink <joris@openbsd.org>
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

static void history_compress(char *, const char *);

struct cvs_cmd cvs_cmd_history = {CVS_OP_HISTORY, CVS_USE_WDIR, "history",
    {"hi", "his"}, /* omghi2you */
    "Display history of actions done in the base repository", "[-ac]", "ac",
    NULL, cvs_history};

/* keep in sync with the defines for history stuff in cvs.h */
const char historytab[] = {
    'T', 'O', 'E', 'F', 'W', 'U', 'G', 'C', 'M', 'A', 'R', '\0'};

#define HISTORY_ALL_USERS 0x01
#define HISTORY_DISPLAY_ARCHIVED 0x02

void
cvs_history_add(int type, struct cvs_file *cf, const char *argument)
{
	BUF	      *buf;
	FILE	      *fp;
	RCSNUM	      *hrev;
	size_t	       len;
	int	       fd;
	char	      *cwd, *p, *rev, *login;
	struct passwd *password;
	char	       revbuf[CVS_REV_BUFSZ], repo[PATH_MAX], fpath[PATH_MAX];
	char	       timebuf[CVS_TIME_BUFSZ];
	struct tm      datetm;

	if (cvs_nolog == 1)
		return;
	if (type < 0 || (size_t)type >= sizeof(historytab) - 1)
		fatal("history: invalid event type");

	if (cvs_cmdop == CVS_OP_CHECKOUT || cvs_cmdop == CVS_OP_EXPORT) {
		if (type != CVS_HISTORY_CHECKOUT && type != CVS_HISTORY_EXPORT)
			return;
	}

	cvs_log(LP_TRACE, "cvs_history_add(`%c', `%s', `%s')", historytab[type],
	    (cf != NULL) ? cf->file_name : "",
	    argument != NULL ? argument : "");

	/* construct repository field */
	if (cvs_cmdop != CVS_OP_CHECKOUT && cvs_cmdop != CVS_OP_EXPORT) {
		cvs_get_repository_name(
		    (cf != NULL) ? cf->file_wd : ".", repo, sizeof(repo));
	} else {
		cvs_get_repository_name(argument, repo, sizeof(repo));
	}

	if (cvs_server_active == 1) {
		cwd = "<remote>";
	} else {
		if (getcwd(fpath, sizeof(fpath)) == NULL)
			fatal("cvs_history_add: getcwd: %s", strerror(errno));
		p = fpath;
		if (cvs_cmdop == CVS_OP_CHECKOUT ||
		    cvs_cmdop == CVS_OP_EXPORT) {
			if (strlcat(fpath, "/", sizeof(fpath)) >=
				sizeof(fpath) ||
			    strlcat(fpath, argument, sizeof(fpath)) >=
				sizeof(fpath))
				fatal("cvs_history_add: string truncation");
		}
		if (cvs_homedir != NULL && cvs_homedir[0] != '\0') {
			len = strlen(cvs_homedir);
			if (strncmp(cvs_homedir, fpath, len) == 0 &&
			    fpath[len] == '/') {
				p += len - 1;
				*p = '~';
			}
		}

		history_compress(p, repo);
		cwd = xstrdup(p);
	}

	/* construct revision field */
	revbuf[0] = '\0';
	rev = revbuf;
	switch (type) {
	case CVS_HISTORY_TAG:
		strlcpy(revbuf, argument, sizeof(revbuf));
		break;
	case CVS_HISTORY_CHECKOUT:
	case CVS_HISTORY_EXPORT:
		/*
		 * buf_alloc uses xcalloc(), so we are safe even
		 * if neither cvs_specified_tag nor cvs_specified_date
		 * have been supplied.
		 */
		buf = buf_alloc(128);
		if (cvs_specified_tag != NULL) {
			buf_puts(buf, cvs_specified_tag);
			if (cvs_specified_date != -1)
				buf_putc(buf, ':');
		}
		if (cvs_specified_date != -1) {
			gmtime_r(&cvs_specified_date, &datetm);
			strftime(timebuf, sizeof(timebuf), "%Y.%m.%d.%H.%M.%S",
			    &datetm);
			buf_puts(buf, timebuf);
		}
		rev = buf_release(buf);
		break;
	case CVS_HISTORY_UPDATE_MERGED:
	case CVS_HISTORY_UPDATE_MERGED_ERR:
	case CVS_HISTORY_COMMIT_MODIFIED:
	case CVS_HISTORY_COMMIT_ADDED:
	case CVS_HISTORY_COMMIT_REMOVED:
	case CVS_HISTORY_UPDATE_CO:
		if ((hrev = rcs_head_get(cf->file_rcs)) == NULL)
			fatal("cvs_history_add: rcs_head_get failed");
		rcsnum_tostr(hrev, revbuf, sizeof(revbuf));
		free(hrev);
		break;
	}

	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s", current_cvsroot->cr_dir,
	    CVS_PATH_HISTORY);

	if ((fd = open(fpath, O_WRONLY | O_APPEND)) == -1) {
		if (errno != ENOENT)
			cvs_log(LP_ERR, "failed to open history file");
	} else {
		if ((fp = fdopen(fd, "a")) != NULL) {
#ifdef _WIN32
			login = NULL;
#else
			login = getlogin();
#endif
			if (login == NULL) {
				password = getpwuid(getuid());
				if (password == NULL)
					fatal("history: cannot determine "
					      "account name");
				login = password->pw_name;
			}
			fprintf(fp, "%c%08llx|%s|%s|%s|%s|%s\n",
			    historytab[type], (long long)time(NULL), login, cwd,
			    repo, rev, (cf != NULL) ? cf->file_name : argument);
			(void)fclose(fp);
		} else {
			cvs_log(LP_ERR, "failed to add entry to history file");
			(void)close(fd);
		}
	}

	if (rev != revbuf)
		free(rev);
	if (cvs_server_active != 1)
		free(cwd);
}

static void
history_compress(char *wdir, const char *repo)
{
	char   offset[2 * sizeof(size_t) + 2];
	size_t repo_len, wdir_len, common;
	int    count;

	repo_len = strlen(repo);
	wdir_len = strlen(wdir);
	common = 0;
	while (common < wdir_len && common < repo_len &&
	    wdir[wdir_len - common - 1] == repo[repo_len - common - 1])
		common++;
	if (common < 3)
		return;
	count = snprintf(offset, sizeof(offset), "*%zx", repo_len - common);
	if (count < 0 || (size_t)count >= sizeof(offset) ||
	    (size_t)count >= common)
		return;
	memcpy(wdir + wdir_len - common, offset, (size_t)count + 1);
}

int
cvs_history(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, cvs_cmd_history.cmd_opts)) != -1) {
		switch (ch) {
		case 'a':
			break;
		case 'c':
			break;
		default:
			fatal("%s", cvs_cmd_history.cmd_synopsis);
		}
	}

	/* Upstream has no history-report implementation; never report false
	 * success. */
	cvs_log(LP_ERR, "history reporting is not implemented");
	return (1);
}
