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
/*	$OpenBSD: logmsg.c,v 1.61 2020/10/19 19:51:20 naddy Exp $	*/
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
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <paths.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "pc_process.h"

#define CVS_LOGMSG_PREFIX		"CVS:"
#define CVS_LOGMSG_LINE		\
"----------------------------------------------------------------------"

int	cvs_logmsg_edit(const char *);

char *
cvs_logmsg_read(const char *path)
{
	int fd;
	BUF *bp;
	FILE *fp;
	size_t len;
	struct stat st;
	char *buf, *lbuf;

	if ((fd = open(path, O_RDONLY)) == -1)
		fatal("cvs_logmsg_read: open %s", strerror(errno));

	if (fstat(fd, &st) == -1)
		fatal("cvs_logmsg_read: fstat %s", strerror(errno));

	if (!S_ISREG(st.st_mode))
		fatal("cvs_logmsg_read: file is not a regular file");

	if ((fp = fdopen(fd, "r")) == NULL)
		fatal("cvs_logmsg_read: fdopen %s", strerror(errno));

	if ((uintmax_t)st.st_size > SIZE_MAX)
		fatal("cvs_logmsg_read: %s: file size too big", path);

	lbuf = NULL;
	bp = buf_alloc(st.st_size);
	while ((buf = pc_metadata_getln(fp, &len))) {
		if (buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
			--len;
		} else {
			lbuf = xmalloc(len + 1);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		if (!strncmp(buf, CVS_LOGMSG_PREFIX,
		    sizeof(CVS_LOGMSG_PREFIX) - 1))
			continue;

		buf_append(bp, buf, len);
		buf_putc(bp, '\n');
	}

	free(lbuf);

	(void)fclose(fp);

	buf_putc(bp, '\0');
	return (buf_release(bp));
}

char *
cvs_logmsg_create(char *dir, struct cvs_flisthead *added,
    struct cvs_flisthead *removed, struct cvs_flisthead *modified)
{
	FILE *fp, *rp;
	int c, fd, rd, saved_errno;
	struct cvs_filelist *cf;
	struct stat st1, st2;
	char *fpath, *logmsg, repo[PATH_MAX];
	char *f, path[PATH_MAX];
	unsigned char block[65536];
	size_t count;
	struct stat st;
	struct trigger_list *line_list;
	struct trigger_line *line;
	static int reuse = 0;
	static char *prevmsg = NULL;

	if (reuse)
		return xstrdup(prevmsg);

	(void)xasprintf(&fpath, "%s/cvsXXXXXXXXXX", cvs_tmpdir);

	if ((fd = mkstemp(fpath)) == -1)
		fatal("cvs_logmsg_create: mkstemp %s", strerror(errno));

	worklist_add(fpath, &temp_files);

	if ((fp = fdopen(fd, "w")) == NULL) {
		saved_errno = errno;
		(void)unlink(fpath);
		fatal("cvs_logmsg_create: fdopen %s", strerror(saved_errno));
	}

	if (prevmsg != NULL && prevmsg[0] != '\0')
		fprintf(fp, "%s", prevmsg);
	else
		fputc('\n', fp);

	cvs_get_repository_name(dir != NULL ? dir : ".", repo, sizeof(repo));
	line_list = cvs_trigger_getlines(CVS_PATH_RCSINFO, repo);
	if (line_list != NULL) {
		TAILQ_FOREACH(line, line_list, flist) {
			if ((rd = open(line->line, O_RDONLY)) == -1)
				fatal("cvs_logmsg_create: open %s",
				    strerror(errno));
			if (fstat(rd, &st) == -1)
				fatal("cvs_logmsg_create: fstat %s",
				    strerror(errno));
			if (!S_ISREG(st.st_mode))
				fatal("cvs_logmsg_create: file is not a "
				    "regular file");
			if ((rp = fdopen(rd, "r")) == NULL)
				fatal("cvs_logmsg_create: fdopen %s",
				    strerror(errno));
			/* Stream templates: stat sizes can become stale while reading. */
			while ((count = fread(block, 1, sizeof(block), rp)) != 0)
				if (fwrite(block, 1, count, fp) != count)
					fatal("cvs_logmsg_create: template write: %s",
					    strerror(errno));
			if (ferror(rp))
				fatal("cvs_logmsg_create: template read: %s",
				    strerror(errno));
			if (fclose(rp) == EOF)
				fatal("cvs_logmsg_create: template close: %s",
				    strerror(errno));
		}
		cvs_trigger_freelist(line_list);
	}

	fprintf(fp, "%s %s\n%s Enter Log.  Lines beginning with `%s' are "
	    "removed automatically\n%s \n", CVS_LOGMSG_PREFIX, CVS_LOGMSG_LINE,
	    CVS_LOGMSG_PREFIX, CVS_LOGMSG_PREFIX, CVS_LOGMSG_PREFIX);

	if (cvs_cmdop == CVS_OP_COMMIT) {
		fprintf(fp, "%s Committing in %s\n%s\n", CVS_LOGMSG_PREFIX,
		    dir != NULL ? dir : ".", CVS_LOGMSG_PREFIX);
	}

	if (added != NULL && !RB_EMPTY(added)) {
		fprintf(fp, "%s Added Files:", CVS_LOGMSG_PREFIX);
		RB_FOREACH(cf, cvs_flisthead, added) {
			f = cf->file_path;
			if (dir != NULL) {
				if (strlcpy(path, f, sizeof(path)) >=
				    sizeof(path))
					fatal("cvs_logmsg_create: truncation");
				f = basename(path);
			}
			fprintf(fp, "\n%s \t%s ", CVS_LOGMSG_PREFIX, f);
		}
		fputs("\n", fp);
	}

	if (removed != NULL && !RB_EMPTY(removed)) {
		fprintf(fp, "%s Removed Files:", CVS_LOGMSG_PREFIX);
		RB_FOREACH(cf, cvs_flisthead, removed) {
			f = cf->file_path;
			if (dir != NULL) {
				if (strlcpy(path, f, sizeof(path)) >=
				    sizeof(path))
					fatal("cvs_logmsg_create: truncation");
				f = basename(path);
			}
			fprintf(fp, "\n%s \t%s ", CVS_LOGMSG_PREFIX, f);
		}
		fputs("\n", fp);
	}

	if (modified != NULL && !RB_EMPTY(modified)) {
		fprintf(fp, "%s Modified Files:", CVS_LOGMSG_PREFIX);
		RB_FOREACH(cf, cvs_flisthead, modified) {
			f = cf->file_path;
			if (dir != NULL) {
				if (strlcpy(path, f, sizeof(path)) >=
				    sizeof(path))
					fatal("cvs_logmsg_create: truncation");
				f = basename(path);
			}
			fprintf(fp, "\n%s \t%s ", CVS_LOGMSG_PREFIX, f);
		}
		fputs("\n", fp);
	}

	fprintf(fp, "%s %s\n", CVS_LOGMSG_PREFIX, CVS_LOGMSG_LINE);
	if (fflush(fp) == EOF)
		fatal("cvs_logmsg_create: flush: %s", strerror(errno));

	if (fstat(fd, &st1) == -1) {
		saved_errno = errno;
		(void)unlink(fpath);
		fatal("cvs_logmsg_create: fstat %s", strerror(saved_errno));
	}

	logmsg = NULL;

	for (;;) {
		if (cvs_logmsg_edit(fpath) != 0)
			fatal("log message editor failed; commit aborted");

		if (fstat(fd, &st2) == -1) {
			saved_errno = errno;
			(void)unlink(fpath);
			fatal("cvs_logmsg_create: fstat %s",
			    strerror(saved_errno));
		}

		if (st1.st_mtime != st2.st_mtime) {
			logmsg = cvs_logmsg_read(fpath);
			free(prevmsg);
			prevmsg = xstrdup(logmsg);
			break;
		}

		printf("\nLog message unchanged or not specified\n"
		    "a)bort, c)ontinue, e)dit, !)reuse this message "
		    "unchanged for remaining dirs\nAction: (abort) ");
		(void)fflush(stdout);

		c = getc(stdin);
		if (c == EOF || c == '\n' || c == 'a' || c == 'A') {
			fatal("Aborted by user");
		} else if (c == 'c' || c == 'C') {
			if (prevmsg == NULL)
				prevmsg = xstrdup("");
			logmsg = xstrdup(prevmsg);
			break;
		} else if (c == 'e' || c == 'E') {
			continue;
		} else if (c == '!') {
			reuse = 1;
			if (prevmsg == NULL)
				prevmsg = xstrdup("");
			logmsg = xstrdup(prevmsg);
			break;
		} else {
			cvs_log(LP_ERR, "invalid input");
			continue;
		}
	}

	(void)fclose(fp);
	(void)unlink(fpath);
	free(fpath);

	return (logmsg);
}

/*
 * Execute an editor on the specified pathname, which is interpreted
 * using an argv parser. Flags and quoted arguments may be included.
 * The generated pathname is appended separately; no shell is invoked.
 *
 * Returns -1 on error, or the exit value on success.
 */
int
cvs_logmsg_edit(const char *pathname)
{
	char **arguments;
	int result;

	arguments = pc_command_argv(cvs_editor, pathname);
	if (arguments == NULL)
		return -1;
	result = pc_run((const char *const *)arguments);
	pc_free_argv(arguments);
	return result;
}

int
cvs_logmsg_verify(char *logmsg)
{
	(void)logmsg;
	int fd, ret = 0;
	char *fpath;
	struct trigger_list *line_list;
	struct file_info_list files_info;
	struct file_info *fi;

	line_list = cvs_trigger_getlines(CVS_PATH_VERIFYMSG, "DEFAULT");
	if (line_list != NULL) {
		TAILQ_INIT(&files_info);

		(void)xasprintf(&fpath, "%s/cvsXXXXXXXXXX", cvs_tmpdir);
		if ((fd = mkstemp(fpath)) == -1)
			fatal("cvs_logmsg_verify: mkstemp %s", strerror(errno));

		fi = xcalloc(1, sizeof(*fi));
		fi->file_path = xstrdup(fpath);
		TAILQ_INSERT_TAIL(&files_info, fi, flist);

		if (cvs_trigger_handle(CVS_TRIGGER_VERIFYMSG, NULL, NULL,
		    line_list, &files_info)) {
			cvs_log(LP_ERR, "Log message check failed");
			ret = 1;
		}

		cvs_trigger_freeinfo(&files_info);
		(void)close(fd);
		(void)unlink(fpath);
		free(fpath);
		cvs_trigger_freelist(line_list);
	}

	return ret;
}

