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
/*	$OpenBSD: config.c,v 1.17 2015/11/05 09:48:21 nicm Exp $	*/
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

#include <sys/types.h>
#include <sys/resource.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "cvs.h"

void
cvs_parse_configfile(void)
{
	cvs_log(LP_TRACE, "cvs_parse_configfile()");
	cvs_read_config(CVS_PATH_CONFIG, config_parse_line);
}

int
config_parse_line(char *line, int lineno)
{
	(void)lineno;
	struct rlimit rl;
	const char   *errstr;
	char	     *val, *opt, *ep;

	opt = line;
	if ((val = strrchr(opt, '=')) == NULL)
		fatal("cvs_parse_configfile: bad option '%s'", opt);

	*(val++) = '\0';

	if (!strcmp(opt, "tag")) {
		free(cvs_tagname);
		cvs_tagname = xstrdup(val);
	} else if (!strcmp(opt, "umask")) {
		long mask;

		errno = 0;
		mask = strtol(val, &ep, 8);

		if (errno == ERANGE || val == ep || *ep != '\0' || mask < 0 ||
		    mask > 07777)
			fatal("cvs_parse_configfile: umask %s is "
			      "invalid",
			    val);
		cvs_umask = (int)mask;
	} else if (!strcmp(opt, "dlimit")) {
		uintmax_t kib, bytes;
		rlim_t	  limit;

		kib = (uintmax_t)strtonum(val, 0, INT_MAX, &errstr);
		if (errstr != NULL)
			fatal("cvs_parse_configfile: dlimit %s is %s", val,
			    errstr);
		if (kib > UINTMAX_MAX / 1024)
			fatal("cvs_parse_configfile: dlimit exceeds range");
		bytes = kib * 1024;
		limit = (rlim_t)bytes;
		if ((uintmax_t)limit != bytes || limit == RLIM_INFINITY)
			fatal("cvs_parse_configfile: dlimit exceeds range");
		if (getrlimit(RLIMIT_DATA, &rl) == -1)
			fatal("cannot read data limit: %s", strerror(errno));
		if (rl.rlim_max != RLIM_INFINITY && limit > rl.rlim_max)
			fatal("dlimit exceeds the hard data limit");
		rl.rlim_cur = limit;
		if (setrlimit(RLIMIT_DATA, &rl) == -1)
			fatal("cannot apply dlimit: %s", strerror(errno));
	} else {
		cvs_log(LP_ERR, "ignoring unknown option '%s'", opt);
	}

	return (0);
}

void
cvs_read_config(char *name, int (*cb)(char *, int))
{
	FILE  *fp;
	size_t len;
	int    lineno;
	char  *p, *buf, *lbuf, fpath[PATH_MAX];

	(void)xsnprintf(
	    fpath, sizeof(fpath), "%s/%s", current_cvsroot->cr_dir, name);

	if ((fp = fopen(fpath, "r")) == NULL)
		return;

	lbuf = NULL;
	lineno = 0;
	while ((buf = pc_metadata_getln(fp, &len)) != NULL) {
		lineno++;
		if (buf[len - 1] == '\n') {
			buf[len - 1] = '\0';
		} else {
			lbuf = xmalloc(len + 1);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		p = buf;
		while (*p == ' ' || *p == '\t')
			p++;

		if (p[0] == '#' || p[0] == '\0')
			continue;

		if (cb(p, lineno) < 0)
			break;
	}

	free(lbuf);
	(void)fclose(fp);
}
