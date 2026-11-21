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
/*	$OpenBSD: entries.c,v 1.107 2016/10/13 20:51:25 fcambus Exp $	*/
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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

#define CVS_ENTRIES_NFIELDS 6
#define CVS_ENTRIES_DELIM '/'

static struct cvs_ent_line *ent_get_line(CVSENTRIES *, const char *);

static void
ent_forget_lines(CVSENTRIES *ep, const char *name)
{
	struct cvs_ent_line *line;

	while ((line = ent_get_line(ep, name)) != NULL) {
		TAILQ_REMOVE(&ep->cef_ent, line, entries_list);
		free(line->buf);
		free(line);
	}
}

static void
ent_append_log(CVSENTRIES *ep, int operation, const char *line)
{
	FILE *fp;
	int   saved = 0;

	if ((fp = fopen(ep->cef_lpath, "a")) == NULL)
		fatal("open %s: %s", ep->cef_lpath, strerror(errno));
	if (fprintf(fp, "%c %s\n", operation, line) < 0 || fflush(fp) == EOF ||
	    fsync(fileno(fp)) == -1)
		saved = errno != 0 ? errno : EIO;
	if (fclose(fp) == EOF && saved == 0)
		saved = errno != 0 ? errno : EIO;
	if (saved != 0)
		fatal("append %s: %s", ep->cef_lpath, strerror(saved));
	if (!ep->cef_modified && pc_sync_parent_directory(ep->cef_lpath) == -1)
		fatal("appended %s but could not sync its directory: %s",
		    ep->cef_lpath, strerror(errno));
	ep->cef_modified = 1;
}

/* Entries and Tag use the same six decimal fields in UTC. */
static time_t
entries_date(const char *text)
{
	static const int min[] = {1, 1, 1, 0, 0, 0};
	static const int max[] = {9999, 12, 31, 23, 59, 59};
	struct tm	 date = {0}, roundtrip;
	int		 fields[6], i, value, digit;
	time_t		 stamp;

	for (i = 0; i < 6; i++) {
		value = 0;
		if (*text < '0' || *text > '9')
			fatal("wrong date specification");
		while (*text >= '0' && *text <= '9') {
			digit = *text++ - '0';
			if (value > max[i] / 10 ||
			    (value == max[i] / 10 && digit > max[i] % 10))
				fatal("date field out of range");
			value = value * 10 + digit;
		}
		if (value < min[i] || *text != (i == 5 ? '\0' : '.'))
			fatal("wrong date specification");
		fields[i] = value;
		if (i != 5)
			text++;
	}

	date.tm_year = fields[0] - 1900;
	date.tm_mon = fields[1] - 1;
	date.tm_mday = fields[2];
	date.tm_hour = fields[3];
	date.tm_min = fields[4];
	date.tm_sec = fields[5];
	stamp = timegm(&date);
	if (gmtime_r(&stamp, &roundtrip) == NULL ||
	    roundtrip.tm_year != fields[0] - 1900 ||
	    roundtrip.tm_mon != fields[1] - 1 ||
	    roundtrip.tm_mday != fields[2] || roundtrip.tm_hour != fields[3] ||
	    roundtrip.tm_min != fields[4] || roundtrip.tm_sec != fields[5])
		fatal("invalid or unrepresentable date");
	return stamp;
}

CVSENTRIES *current_list = NULL;

CVSENTRIES *
cvs_ent_open(const char *dir)
{
	FILE	       *fp;
	CVSENTRIES     *ep;
	char	       *p, buf[PATH_MAX], cwd[PATH_MAX], absolute[PATH_MAX];
	struct cvs_ent *ent;
	struct cvs_ent_line *line;

	cvs_log(LP_TRACE, "cvs_ent_open(%s)", dir);

	/* Cached entries can be flushed after a server command changes cwd. */
	if (!pc_native_absolute(dir)) {
		if (getcwd(cwd, sizeof(cwd)) == NULL)
			fatal("cvs_ent_open: getcwd: %s", strerror(errno));
		(void)xsnprintf(absolute, sizeof(absolute), "%s/%s", cwd, dir);
		dir = absolute;
	}
	(void)xsnprintf(buf, sizeof(buf), "%s/%s", dir, CVS_PATH_ENTRIES);

	if (current_list != NULL && !strcmp(current_list->cef_path, buf))
		return (current_list);

	if (current_list != NULL) {
		cvs_ent_close(current_list, ENT_SYNC);
		current_list = NULL;
	}

	ep = xcalloc(1, sizeof(*ep));
	ep->cef_path = xstrdup(buf);

	(void)xsnprintf(buf, sizeof(buf), "%s/%s", dir, CVS_PATH_BACKUPENTRIES);

	ep->cef_bpath = xstrdup(buf);

	(void)xsnprintf(buf, sizeof(buf), "%s/%s", dir, CVS_PATH_LOGENTRIES);

	ep->cef_lpath = xstrdup(buf);

	TAILQ_INIT(&(ep->cef_ent));

	if ((fp = fopen(ep->cef_path, "r")) != NULL) {
		while (pc_metadata_fgets(buf, sizeof(buf), fp)) {
			buf[strcspn(buf, "\n")] = '\0';

			if (buf[0] == 'D' && buf[1] == '\0')
				break;

			line = xmalloc(sizeof(*line));
			line->buf = xstrdup(buf);
			TAILQ_INSERT_TAIL(&(ep->cef_ent), line, entries_list);
		}

		if (fclose(fp) == EOF)
			fatal("close %s: %s", ep->cef_path, strerror(errno));
	} else if (errno != ENOENT)
		fatal("open %s: %s", ep->cef_path, strerror(errno));

	if ((fp = fopen(ep->cef_lpath, "r")) != NULL) {
		ep->cef_modified = 1;
		while (pc_metadata_fgets(buf, sizeof(buf), fp)) {
			buf[strcspn(buf, "\n")] = '\0';

			if ((buf[0] != 'A' && buf[0] != 'R') || buf[1] != ' ')
				fatal("cvs_ent_open: %s: malformed line %s",
				    ep->cef_lpath, buf);

			p = &buf[2];
			if ((ent = cvs_ent_parse(p)) == NULL)
				fatal("cvs_ent_open: invalid entry in %s",
				    ep->cef_lpath);
			ent_forget_lines(ep, ent->ce_name);
			cvs_ent_free(ent);

			if (buf[0] == 'A') {
				line = xmalloc(sizeof(*line));
				line->buf = xstrdup(p);
				TAILQ_INSERT_TAIL(
				    &(ep->cef_ent), line, entries_list);
			}
		}

		if (fclose(fp) == EOF)
			fatal("close %s: %s", ep->cef_lpath, strerror(errno));
	} else if (errno != ENOENT)
		fatal("open %s: %s", ep->cef_lpath, strerror(errno));

	current_list = ep;
	return (ep);
}

struct cvs_ent *
cvs_ent_parse(const char *entry)
{
	int		i;
	struct tm	t = {0};
	struct cvs_ent *ent;
	char	       *fields[CVS_ENTRIES_NFIELDS], *buf, *sp, *dp, *p;

	buf = sp = xstrdup(entry);
	i = 0;
	do {
		dp = strchr(sp, CVS_ENTRIES_DELIM);
		if (dp != NULL)
			*(dp++) = '\0';
		fields[i++] = sp;
		sp = dp;
	} while (dp != NULL && i < CVS_ENTRIES_NFIELDS);

	if (i < CVS_ENTRIES_NFIELDS)
		fatal("missing fields in entry line '%s'", entry);

	ent = xmalloc(sizeof(*ent));
	ent->ce_buf = buf;

	if (*fields[0] == '\0')
		ent->ce_type = CVS_ENT_FILE;
	else if (*fields[0] == 'D')
		ent->ce_type = CVS_ENT_DIR;
	else
		ent->ce_type = CVS_ENT_NONE;

	ent->ce_status = CVS_ENT_REG;
	ent->ce_name = fields[1];
	ent->ce_rev = NULL;
	ent->ce_date = -1;
	ent->ce_tag = NULL;
	ent->ce_time = NULL;
	ent->ce_mtime = CVS_DATE_DMSEC;

	if (ent->ce_type == CVS_ENT_FILE) {
		if (*fields[2] == '-') {
			ent->ce_status = CVS_ENT_REMOVED;
			sp = fields[2] + 1;
		} else if (*fields[2] == CVS_SERVER_QUESTIONABLE) {
			sp = NULL;
			ent->ce_status = CVS_ENT_UNKNOWN;
		} else {
			sp = fields[2];
			if (fields[2][0] == '0' && fields[2][1] == '\0')
				ent->ce_status = CVS_ENT_ADDED;
		}

		if (sp != NULL) {
			if ((ent->ce_rev = rcsnum_parse(sp)) == NULL) {
				fatal("failed to parse entry revision '%s'",
				    entry);
			}
		}

		if (fields[3][0] == '\0' ||
		    strncmp(fields[3], CVS_DATE_DUMMY,
			sizeof(CVS_DATE_DUMMY) - 1) == 0 ||
		    strncmp(fields[3], "Initial ", 8) == 0 ||
		    strcmp(fields[3], "Result of merge") == 0) {
			ent->ce_mtime = CVS_DATE_DMSEC;
		} else if (cvs_server_active == 1 &&
		    strncmp(fields[3], CVS_SERVER_UNCHANGED,
			strlen(CVS_SERVER_UNCHANGED)) == 0) {
			ent->ce_mtime = CVS_SERVER_UPTODATE;
		} else {
			p = fields[3];
			if (strncmp(fields[3], "Result of merge+", 16) == 0)
				p += 16;

			ent->ce_time = xstrdup(p);

			/* Date field can be a '+=' with remote to indicate
			 * conflict.  In this case do nothing. */
			if (strptime(p, "%a %b %d %T %Y", &t) != NULL) {
				t.tm_isdst = 0;
				ent->ce_mtime = timegm(&t);
			}
		}
	}

	ent->ce_conflict = fields[3];
	if ((dp = strchr(ent->ce_conflict, '+')) != NULL)
		*dp = '\0';
	else
		ent->ce_conflict = NULL;

	if (strcmp(fields[4], ""))
		ent->ce_opts = fields[4];
	else
		ent->ce_opts = NULL;

	if (strcmp(fields[5], "")) {
		switch (*fields[5]) {
		case 'D':
			ent->ce_date = entries_date(fields[5] + 1);
			ent->ce_tag = NULL;
			break;
		case 'T':
			ent->ce_tag = fields[5] + 1;
			break;
		default:
			fatal("invalid sticky entry");
		}
	}

	return (ent);
}

struct cvs_ent *
cvs_ent_get(CVSENTRIES *ep, const char *name)
{
	struct cvs_ent	    *ent;
	struct cvs_ent_line *l;

	l = ent_get_line(ep, name);
	if (l == NULL)
		return (NULL);

	ent = cvs_ent_parse(l->buf);
	return (ent);
}

void
cvs_ent_close(CVSENTRIES *ep, int writefile)
{
	FILE		    *fp;
	struct cvs_ent_line *l;
	int		     dflag, fd, saved;
	char		    *temporary = NULL;

	dflag = 1;
	cvs_log(LP_TRACE, "cvs_ent_close(%s, %d)", ep->cef_bpath, writefile);

	if (cvs_cmdop == CVS_OP_EXPORT || cvs_cmdop == CVS_OP_IMPORT ||
	    !ep->cef_modified)
		writefile = 0;

	fp = NULL;
	if (writefile) {
		xasprintf(&temporary, "%s.XXXXXXXX", ep->cef_bpath);
		if ((fd = mkstemp(temporary)) == -1)
			fatal("cvs_ent_close: create temporary: %s",
			    strerror(errno));
		worklist_add(temporary, &temp_files);
		if ((fp = fdopen(fd, "w")) == NULL)
			fatal("cvs_ent_close: fdopen: %s", strerror(errno));
	}

	while ((l = TAILQ_FIRST(&(ep->cef_ent))) != NULL) {
		if (fp != NULL) {
			if (l->buf[0] == 'D')
				dflag = 0;

			fputs(l->buf, fp);
			fputc('\n', fp);
		}

		TAILQ_REMOVE(&(ep->cef_ent), l, entries_list);
		free(l->buf);
		free(l);
	}

	if (fp != NULL) {
		if (dflag) {
			fputc('D', fp);
			fputc('\n', fp);
		}
		if (fflush(fp) == EOF || ferror(fp) ||
		    fsync(fileno(fp)) == -1) {
			saved = errno;
			(void)fclose(fp);
			(void)unlink(temporary);
			fatal("cvs_ent_close: flush: %s", strerror(saved));
		}
		if (fclose(fp) == EOF) {
			saved = errno;
			(void)unlink(temporary);
			fatal("cvs_ent_close: close: %s", strerror(saved));
		}

		if (rename(temporary, ep->cef_path) == -1)
			fatal("cvs_ent_close: rename: `%s'->`%s': %s",
			    temporary, ep->cef_path, strerror(errno));
		worklist_remove(temporary, &temp_files);
		free(temporary);

		if (pc_sync_parent_directory(ep->cef_path) == -1)
			fatal(
			    "published %s but could not sync its directory: %s",
			    ep->cef_path, strerror(errno));
		if (unlink(ep->cef_lpath) == -1 && errno != ENOENT)
			fatal("cvs_ent_close: remove log: %s", strerror(errno));
		if (pc_sync_parent_directory(ep->cef_path) == -1)
			fatal(
			    "compacted %s but could not sync its directory: %s",
			    ep->cef_path, strerror(errno));
	}

	if (current_list == ep)
		current_list = NULL;
	free(ep->cef_path);
	free(ep->cef_bpath);
	free(ep->cef_lpath);
	free(ep);
}

void
cvs_ent_add(CVSENTRIES *ep, const char *line)
{
	struct cvs_ent_line *l;
	struct cvs_ent	    *ent;

	if ((ent = cvs_ent_parse(line)) == NULL)
		fatal("cvs_ent_add: parsing failed '%s'", line);

	if (cvs_server_active == 0)
		cvs_log(LP_TRACE, "cvs_ent_add(%s, %s)", ep->cef_path, line);

	ent_append_log(ep, 'A', line);
	l = xmalloc(sizeof(*l));
	l->buf = xstrdup(line);
	ent_forget_lines(ep, ent->ce_name);
	cvs_ent_free(ent);
	TAILQ_INSERT_TAIL(&(ep->cef_ent), l, entries_list);
}

void
cvs_ent_remove(CVSENTRIES *ep, const char *name)
{
	struct cvs_ent_line *l;

	if (cvs_server_active == 0)
		cvs_log(LP_TRACE, "cvs_ent_remove(%s, %s)", ep->cef_path, name);

	l = ent_get_line(ep, name);
	if (l == NULL)
		return;

	ent_append_log(ep, 'R', l->buf);
	ent_forget_lines(ep, name);
}

/*
 * cvs_ent_line_str()
 *
 * Build CVS/Entries line.
 *
 */
void
cvs_ent_line_str(const char *name, char *rev, char *tstamp, char *opts,
    char *sticky, int isdir, int isremoved, char *buf, size_t len)
{
	if (isdir == 1) {
		(void)xsnprintf(buf, len, "D/%s////", name);
		return;
	}

	(void)xsnprintf(buf, len, "/%s/%s%s/%s/%s/%s", name,
	    isremoved == 1 ? "-" : "", rev, tstamp, opts, sticky);
}

void
cvs_ent_free(struct cvs_ent *ent)
{
	free(ent->ce_rev);
	free(ent->ce_time);
	free(ent->ce_buf);
	free(ent);
}

static struct cvs_ent_line *
ent_get_line(CVSENTRIES *ep, const char *name)
{
	char		    *p, *s;
	struct cvs_ent_line *l;

	TAILQ_FOREACH(l, &(ep->cef_ent), entries_list)
	{
		if (l->buf[0] == 'D')
			p = &(l->buf[2]);
		else
			p = &(l->buf[1]);

		if ((s = strchr(p, '/')) == NULL)
			fatal("ent_get_line: bad entry line '%s'", l->buf);

		*s = '\0';

		if (!strcmp(p, name)) {
			*s = '/';
			return (l);
		}

		*s = '/';
	}

	return (NULL);
}

void
cvs_parse_tagfile(char *dir, char **tagp, char **datep, int *nbp)
{
	FILE  *fp;
	int    i, linenum;
	size_t len;
	char   linebuf[128], tagpath[PATH_MAX];

	cvs_directory_date = -1;

	if (tagp != NULL)
		*tagp = NULL;

	if (datep != NULL)
		*datep = NULL;

	if (nbp != NULL)
		*nbp = 0;

	i = snprintf(tagpath, PATH_MAX, "%s/%s", dir, CVS_PATH_TAG);
	if (i < 0 || i >= PATH_MAX)
		return;

	if ((fp = fopen(tagpath, "r")) == NULL) {
		if (errno != ENOENT)
			cvs_log(LP_NOTICE, "failed to open `%s' : %s", tagpath,
			    strerror(errno));
		return;
	}

	linenum = 0;

	while (pc_metadata_fgets(linebuf, (int)sizeof(linebuf), fp) != NULL) {
		linenum++;
		if ((len = strlen(linebuf)) == 0)
			continue;
		if (linebuf[len - 1] != '\n') {
			cvs_log(LP_NOTICE, "line too long in `%s:%d'", tagpath,
			    linenum);
			break;
		}
		linebuf[--len] = '\0';

		switch (*linebuf) {
		case 'T':
			if (tagp != NULL)
				*tagp = xstrdup(linebuf + 1);
			break;
		case 'D':
			cvs_directory_date = entries_date(linebuf + 1);

			if (datep != NULL)
				*datep = xstrdup(linebuf + 1);
			break;
		case 'N':
			if (tagp != NULL)
				*tagp = xstrdup(linebuf + 1);
			if (nbp != NULL)
				*nbp = 1;
			break;
		default:
			break;
		}
	}
	if (ferror(fp))
		cvs_log(LP_NOTICE, "failed to read line from `%s'", tagpath);

	(void)fclose(fp);
}

void
cvs_write_tagfile(const char *dir, char *tag, char *date)
{
	FILE	 *fp;
	RCSNUM	 *rev;
	char	  tagpath[PATH_MAX];
	char	  sticky[CVS_REV_BUFSZ];
	struct tm datetm;
	int	  i;

	cvs_log(LP_TRACE, "cvs_write_tagfile(%s, %s, %s)", dir,
	    tag != NULL ? tag : "", date != NULL ? date : "");

	if (cvs_noexec == 1)
		return;

	i = snprintf(tagpath, PATH_MAX, "%s/%s", dir, CVS_PATH_TAG);
	if (i < 0 || i >= PATH_MAX)
		return;

	if (tag != NULL || cvs_specified_date != -1 ||
	    cvs_directory_date != -1) {
		if ((fp = fopen(tagpath, "w+")) == NULL) {
			if (errno != ENOENT) {
				cvs_log(LP_NOTICE, "failed to open `%s' : %s",
				    tagpath, strerror(errno));
			}
			return;
		}

		if (tag != NULL) {
			if ((rev = rcsnum_parse(tag)) != NULL) {
				(void)xsnprintf(
				    sticky, sizeof(sticky), "N%s", tag);
				free(rev);
			} else {
				(void)xsnprintf(
				    sticky, sizeof(sticky), "T%s", tag);
			}
		} else {
			if (cvs_specified_date != -1)
				gmtime_r(&cvs_specified_date, &datetm);
			else
				gmtime_r(&cvs_directory_date, &datetm);
			(void)strftime(
			    sticky, sizeof(sticky), "D" CVS_DATE_FMT, &datetm);
		}

		(void)fprintf(fp, "%s\n", sticky);
		(void)fclose(fp);
	}
}
