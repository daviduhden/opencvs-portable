/* Portable OpenCVS modifications.
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
/*	$OpenBSD: rcsparse.c,v 1.13 2016/10/13 20:51:25 fcambus Exp $	*/
/*
 * Copyright (c) 2010 Tobias Stoeckmann <tobias@openbsd.org>
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

#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "rcs.h"
#include "rcsparse.h"
#include "xmalloc.h"

#define RCS_BUFSIZE 16384
#define RCS_BUFEXTSIZE 8192

/* RCS token types */
#define RCS_TOK_HEAD (1 << 0)
#define RCS_TOK_BRANCH (1 << 1)
#define RCS_TOK_ACCESS (1 << 2)
#define RCS_TOK_SYMBOLS (1 << 3)
#define RCS_TOK_LOCKS (1 << 4)
#define RCS_TOK_STRICT (1 << 5)
#define RCS_TOK_COMMENT (1 << 6)
#define RCS_TOK_COMMITID (1 << 7)
#define RCS_TOK_EXPAND (1 << 8)
#define RCS_TOK_DESC (1 << 9)
#define RCS_TOK_DATE (1 << 10)
#define RCS_TOK_AUTHOR (1 << 11)
#define RCS_TOK_STATE (1 << 12)
#define RCS_TOK_BRANCHES (1 << 13)
#define RCS_TOK_NEXT (1 << 14)
#define RCS_TOK_LOG (1 << 15)
#define RCS_TOK_TEXT (1 << 16)
#define RCS_TOK_COLON (1 << 17)
#define RCS_TOK_COMMA (1 << 18)
#define RCS_TOK_SCOLON (1 << 19)

#define RCS_TYPE_STRING (1 << 20)
#define RCS_TYPE_NUMBER (1 << 21)
#define RCS_TYPE_BRANCH (1 << 22)
#define RCS_TYPE_REVISION (1 << 23)
#define RCS_TYPE_LOGIN (1 << 24)
#define RCS_TYPE_STATE (1 << 25)
#define RCS_TYPE_SYMBOL (1 << 26)
#define RCS_TYPE_DATE (1 << 27)
#define RCS_TYPE_KEYWORD (1 << 28)
#define RCS_TYPE_COMMITID (1 << 29)

#define RCS_MANDATORY 0
#define RCS_OPTIONAL 1

/* opaque parse data */
struct rcs_pdata {
	char  *rp_buf;
	size_t rp_blen;
	char  *rp_bufend;
	size_t rp_tlen;

	struct rcs_delta *rp_delta;
	int		  rp_lineno;
	int		  rp_msglineno;
	int		  rp_token;

	union {
		RCSNUM	 *rev;
		char	 *str;
		struct tm date;
	} rp_value;
};

struct rcs_keyword {
	const char *k_name;
	int	    k_val;
};

struct rcs_section {
	int token;
	int (*parse)(RCSFILE *, struct rcs_pdata *);
	int opt;
};

/* this has to be sorted always */
static const struct rcs_keyword keywords[] = {{"access", RCS_TOK_ACCESS},
    {"author", RCS_TOK_AUTHOR}, {"branch", RCS_TOK_BRANCH},
    {"branches", RCS_TOK_BRANCHES}, {"comment", RCS_TOK_COMMENT},
    {"commitid", RCS_TOK_COMMITID}, {"date", RCS_TOK_DATE},
    {"desc", RCS_TOK_DESC}, {"expand", RCS_TOK_EXPAND}, {"head", RCS_TOK_HEAD},
    {"locks", RCS_TOK_LOCKS}, {"log", RCS_TOK_LOG}, {"next", RCS_TOK_NEXT},
    {"state", RCS_TOK_STATE}, {"strict", RCS_TOK_STRICT},
    {"symbols", RCS_TOK_SYMBOLS}, {"text", RCS_TOK_TEXT}};

/* parser functions specified in rcs_section structs */
static int rcsparse_head(RCSFILE *, struct rcs_pdata *);
static int rcsparse_branch(RCSFILE *, struct rcs_pdata *);
static int rcsparse_access(RCSFILE *, struct rcs_pdata *);
static int rcsparse_symbols(RCSFILE *, struct rcs_pdata *);
static int rcsparse_locks(RCSFILE *, struct rcs_pdata *);
static int rcsparse_strict(RCSFILE *, struct rcs_pdata *);
static int rcsparse_comment(RCSFILE *, struct rcs_pdata *);
static int rcsparse_commitid(RCSFILE *, struct rcs_pdata *);
static int rcsparse_expand(RCSFILE *, struct rcs_pdata *);
static int rcsparse_deltarevision(RCSFILE *, struct rcs_pdata *);
static int rcsparse_date(RCSFILE *, struct rcs_pdata *);
static int rcsparse_author(RCSFILE *, struct rcs_pdata *);
static int rcsparse_state(RCSFILE *, struct rcs_pdata *);
static int rcsparse_branches(RCSFILE *, struct rcs_pdata *);
static int rcsparse_next(RCSFILE *, struct rcs_pdata *);
static int rcsparse_textrevision(RCSFILE *, struct rcs_pdata *);
static int rcsparse_log(RCSFILE *, struct rcs_pdata *);
static int rcsparse_text(RCSFILE *, struct rcs_pdata *);

static int rcsparse_delta(RCSFILE *);
static int rcsparse_deltatext(RCSFILE *);
static int rcsparse_desc(RCSFILE *);

static int  kw_cmp(const void *, const void *);
static int  rcsparse(RCSFILE *, struct rcs_section *);
static void rcsparse_growbuf(RCSFILE *);
static int  rcsparse_string(RCSFILE *, int);
static int  rcsparse_token(RCSFILE *, int);
static void rcsparse_warnx(RCSFILE *, char *, ...) PC_PRINTF(2, 3);
static int  valid_login(char *);
static int  valid_commitid(char *);

struct rcs_revision_ref {
	struct rcs_delta *delta;
	int		  linked;
};

static int
rcsparse_compare_revisions(const void *left, const void *right)
{
	const struct rcs_revision_ref *a = left, *b = right;

	return (rcsnum_cmp(a->delta->rd_num, b->delta->rd_num, 0));
}

static struct rcs_revision_ref *
rcsparse_find_revision(struct rcs_revision_ref *refs, size_t count, RCSNUM *rev)
{
	struct rcs_delta	key = {.rd_num = rev};
	struct rcs_revision_ref ref = {.delta = &key};

	return (bsearch(
	    &ref, refs, count, sizeof(*refs), rcsparse_compare_revisions));
}

/* Trunk edges descend, branch edges ascend, and fork edges go deeper. */
static int
rcsparse_check_graph(RCSFILE *rfp)
{
	struct rcs_revision_ref *refs, *target;
	struct rcs_delta	*rdp;
	struct rcs_branch	*branch;
	RCSNUM			*rev, *next;
	size_t			 count, i;
	int			 valid = 0;

	count = 0;
	TAILQ_FOREACH(rdp, &rfp->rf_delta, rd_list)
	count++;
	if (count == 0)
		return (rfp->rf_head == NULL);
	if (rfp->rf_head == NULL || rfp->rf_head->rn_len != 2)
		return (0);

	refs = xcalloc(count, sizeof(*refs));
	i = 0;
	TAILQ_FOREACH(rdp, &rfp->rf_delta, rd_list)
	refs[i++].delta = rdp;
	qsort(refs, count, sizeof(*refs), rcsparse_compare_revisions);
	target = rcsparse_find_revision(refs, count, rfp->rf_head);
	if (target == NULL)
		goto out;
	target->linked = 1;

	for (i = 0; i < count; i++) {
		rdp = refs[i].delta;
		rev = rdp->rd_num;
		if ((i != 0 &&
			rcsparse_compare_revisions(&refs[i - 1], &refs[i]) ==
			    0) ||
		    rev->rn_len < 2 || RCSNUM_ISBRANCH(rev))
			goto out;

		next = rdp->rd_next;
		if (next->rn_len != 0) {
			if (next->rn_len != rev->rn_len ||
			    (rev->rn_len == 2 &&
				rcsnum_cmp(rev, next, 0) != -1) ||
			    (rev->rn_len > 2 &&
				(rcsnum_cmp(rev, next, rev->rn_len - 1) != 0 ||
				    rcsnum_cmp(rev, next, 0) != 1)))
				goto out;
			target = rcsparse_find_revision(refs, count, next);
			if (target == NULL || target->linked)
				goto out;
			target->linked = 1;
		}
		TAILQ_FOREACH(branch, &rdp->rd_branches, rb_list)
		{
			next = branch->rb_num;
			if (next->rn_len != rev->rn_len + 2 ||
			    rcsnum_cmp(rev, next, rev->rn_len) != 0)
				goto out;
			target = rcsparse_find_revision(refs, count, next);
			if (target == NULL || target->linked)
				goto out;
			target->linked = 1;
		}
	}
	for (i = 0; i < count; i++)
		if (!refs[i].linked)
			goto out;
	valid = 1;
out:
	free(refs);
	return (valid);
}

/*
 * head [REVISION];
 * [branch BRANCH];
 * access [LOGIN ...];
 * symbols [SYMBOL:REVISION ...];
 * locks [LOGIN:REVISION ...];
 * [strict;]
 * [comment [@[...]@];]
 * [expand [@[...]@];]
 */
static struct rcs_section sec_admin[] = {
    {RCS_TOK_HEAD, rcsparse_head, RCS_MANDATORY},
    {RCS_TOK_BRANCH, rcsparse_branch, RCS_OPTIONAL},
    {RCS_TOK_ACCESS, rcsparse_access, RCS_MANDATORY},
    {RCS_TOK_SYMBOLS, rcsparse_symbols, RCS_MANDATORY},
    {RCS_TOK_LOCKS, rcsparse_locks, RCS_MANDATORY},
    {RCS_TOK_STRICT, rcsparse_strict, RCS_OPTIONAL},
    {RCS_TOK_COMMENT, rcsparse_comment, RCS_OPTIONAL},
    {RCS_TOK_EXPAND, rcsparse_expand, RCS_OPTIONAL}, {0, NULL, 0}};

/*
 * REVISION
 * date [YY]YY.MM.DD.HH.MM.SS;
 * author LOGIN;
 * state STATE;
 * branches [REVISION ...];
 * next [REVISION];
 * [commitid ID;]
 */
static struct rcs_section sec_delta[] = {
    {RCS_TYPE_REVISION, rcsparse_deltarevision, RCS_MANDATORY},
    {RCS_TOK_DATE, rcsparse_date, RCS_MANDATORY},
    {RCS_TOK_AUTHOR, rcsparse_author, RCS_MANDATORY},
    {RCS_TOK_STATE, rcsparse_state, RCS_MANDATORY},
    {RCS_TOK_BRANCHES, rcsparse_branches, RCS_MANDATORY},
    {RCS_TOK_NEXT, rcsparse_next, RCS_MANDATORY},
    {RCS_TOK_COMMITID, rcsparse_commitid, RCS_OPTIONAL}, {0, NULL, 0}};

/*
 * REVISION
 * log @[...]@
 * text @[...]@
 */
static struct rcs_section sec_deltatext[] = {
    {RCS_TYPE_REVISION, rcsparse_textrevision, RCS_MANDATORY},
    {RCS_TOK_LOG, rcsparse_log, RCS_MANDATORY},
    {RCS_TOK_TEXT, rcsparse_text, RCS_MANDATORY}, {0, NULL, 0}};

/*
 * rcsparse_init()
 *
 * Initializes the parsing data structure and parses the admin section of
 * RCS file <rfp>.
 *
 * Returns 0 on success or 1 on failure.
 */
int
rcsparse_init(RCSFILE *rfp)
{
	struct rcs_pdata *pdp;

	if (rfp->rf_flags & RCS_PARSED)
		return (0);

	pdp = xcalloc(1, sizeof(*pdp));
	pdp->rp_buf = xmalloc(RCS_BUFSIZE);
	pdp->rp_blen = RCS_BUFSIZE;
	pdp->rp_bufend = pdp->rp_buf + pdp->rp_blen - 1;
	pdp->rp_token = -1;
	pdp->rp_lineno = 1;
	pdp->rp_msglineno = 1;

	/* ditch the strict lock */
	rfp->rf_flags &= ~RCS_SLOCK;
	rfp->rf_pdata = pdp;

	if (rcsparse(rfp, sec_admin)) {
		rcsparse_free(rfp);
		return (1);
	}

	if ((rfp->rf_flags & RCS_PARSE_FULLY) &&
	    rcsparse_deltatexts(rfp, NULL)) {
		rcsparse_free(rfp);
		return (1);
	}

	rfp->rf_flags |= RCS_SYNCED;
	return (0);
}

/*
 * rcsparse_deltas()
 *
 * Parse deltas. If <rev> is not NULL, parse only as far as that
 * revision. If <rev> is NULL, parse all deltas.
 *
 * Returns 0 on success or 1 on error.
 */
int
rcsparse_deltas(RCSFILE *rfp, RCSNUM *rev)
{
	int		  ret;
	struct rcs_delta *enddelta;

	if ((rfp->rf_flags & PARSED_DELTAS) || (rfp->rf_flags & RCS_CREATE))
		return (0);

	for (;;) {
		ret = rcsparse_delta(rfp);
		if (ret < 0)
			return (1);
		if (ret > 0 && rev != NULL) {
			enddelta = TAILQ_LAST(&(rfp->rf_delta), rcs_dlist);
			if (enddelta == NULL)
				return (1);

			if (rcsnum_cmp(enddelta->rd_num, rev, 0) == 0)
				break;
		}

		if (ret == 0) {
			if (!rcsparse_check_graph(rfp)) {
				rcsparse_warnx(
				    rfp, "invalid RCS revision graph");
				return (1);
			}
			rfp->rf_flags |= PARSED_DELTAS;
			break;
		}
	}

	return (0);
}

/*
 * rcsparse_deltatexts()
 *
 * Parse deltatexts. If <rev> is not NULL, parse only as far as that
 * revision. If <rev> is NULL, parse everything.
 *
 * Returns 0 on success or 1 on error.
 */
int
rcsparse_deltatexts(RCSFILE *rfp, RCSNUM *rev)
{
	int		  ret;
	struct rcs_delta *rdp, *delta;

	if ((rfp->rf_flags & PARSED_DELTATEXTS) || (rfp->rf_flags & RCS_CREATE))
		return (0);

	if (!(rfp->rf_flags & PARSED_DESC))
		if (rcsparse_desc(rfp))
			return (1);

	rdp = (rev != NULL) ? rcs_findrev(rfp, rev) : NULL;
	if (rev != NULL && rdp == NULL)
		return (1);

	for (;;) {
		if (rdp != NULL && rdp->rd_text != NULL)
			break;
		ret = rcsparse_deltatext(rfp);
		if (ret == 0) {
			TAILQ_FOREACH(delta, &rfp->rf_delta, rd_list)
			{
				if (delta->rd_text == NULL) {
					rcsparse_warnx(
					    rfp, "missing RCS deltatext");
					return (1);
				}
			}
			rfp->rf_flags |= PARSED_DELTATEXTS;
			break;
		} else if (ret == -1)
			return (1);
	}

	return (0);
}

/*
 * rcsparse_free()
 *
 * Free the contents of the <rfp>'s parser data structure.
 */
void
rcsparse_free(RCSFILE *rfp)
{
	struct rcs_pdata *pdp;

	pdp = rfp->rf_pdata;

	free(pdp->rp_buf);
	if (pdp->rp_token == RCS_TYPE_REVISION)
		free(pdp->rp_value.rev);
	free(pdp);
}

/*
 * rcsparse_desc()
 *
 * Parse desc of the RCS file <rfp>.  By calling rcsparse_desc, all deltas
 * will be parsed in order to proceed the reading cursor to the desc keyword.
 *
 * desc @[...]@;
 *
 * Returns 0 on success or 1 on error.
 */
static int
rcsparse_desc(RCSFILE *rfp)
{
	struct rcs_pdata *pdp;

	if (rfp->rf_flags & PARSED_DESC)
		return (0);

	if (!(rfp->rf_flags & PARSED_DELTAS) && rcsparse_deltas(rfp, NULL))
		return (1);

	pdp = (struct rcs_pdata *)rfp->rf_pdata;

	if (rcsparse_token(rfp, RCS_TOK_DESC) != RCS_TOK_DESC ||
	    rcsparse_token(rfp, RCS_TYPE_STRING) != RCS_TYPE_STRING)
		return (1);

	rfp->rf_desc = pdp->rp_value.str;
	rfp->rf_flags |= PARSED_DESC;

	return (0);
}

/*
 * rcsparse_deltarevision()
 *
 * Called upon reaching a new REVISION entry in the delta section.
 * A new rcs_delta structure will be prepared in pdp->rp_delta for further
 * parsing.
 *
 * REVISION
 *
 * Always returns 0.
 */
static int
rcsparse_deltarevision(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	(void)rfp;
	struct rcs_delta *rdp;

	rdp = xcalloc(1, sizeof(*rdp));
	TAILQ_INIT(&rdp->rd_branches);
	rdp->rd_num = pdp->rp_value.rev;
	pdp->rp_delta = rdp;

	return (0);
}

/*
 * rcsparse_date()
 *
 * Parses the specified date of current delta pdp->rp_delta.
 *
 * date YYYY.MM.DD.HH.MM.SS;
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_date(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_DATE) != RCS_TYPE_DATE)
		return (1);

	pdp->rp_delta->rd_date = pdp->rp_value.date;

	return (rcsparse_token(rfp, RCS_TOK_SCOLON) != RCS_TOK_SCOLON);
}

/*
 * rcsparse_author()
 *
 * Parses the specified author of current delta pdp->rp_delta.
 *
 * author LOGIN;
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_author(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_LOGIN) != RCS_TYPE_LOGIN)
		return (1);

	pdp->rp_delta->rd_author = pdp->rp_value.str;

	return (rcsparse_token(rfp, RCS_TOK_SCOLON) != RCS_TOK_SCOLON);
}

/*
 * rcsparse_state()
 *
 * Parses the specified state of current delta pdp->rp_delta.
 *
 * state STATE;
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_state(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_STATE) != RCS_TYPE_STATE)
		return (1);

	pdp->rp_delta->rd_state = pdp->rp_value.str;

	return (rcsparse_token(rfp, RCS_TOK_SCOLON) != RCS_TOK_SCOLON);
}

/*
 * rcsparse_branches()
 *
 * Parses the specified branches of current delta pdp->rp_delta.
 *
 * branches [REVISION ...];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_branches(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	struct rcs_branch *rb;
	int		   type;

	while ((type = rcsparse_token(rfp,
		    RCS_TOK_SCOLON | RCS_TYPE_REVISION)) == RCS_TYPE_REVISION) {
		rb = xmalloc(sizeof(*rb));
		rb->rb_num = pdp->rp_value.rev;
		TAILQ_INSERT_TAIL(&(pdp->rp_delta->rd_branches), rb, rb_list);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_next()
 *
 * Parses the specified next revision of current delta pdp->rp_delta.
 *
 * next [REVISION];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_next(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	int type;

	type = rcsparse_token(rfp, RCS_TYPE_REVISION | RCS_TOK_SCOLON);
	if (type == RCS_TYPE_REVISION) {
		pdp->rp_delta->rd_next = pdp->rp_value.rev;
		type = rcsparse_token(rfp, RCS_TOK_SCOLON);
	} else
		pdp->rp_delta->rd_next = rcsnum_alloc();

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_commitid()
 *
 * Parses the specified commit id of current delta pdp->rp_delta. The
 * commitid keyword is optional and can be omitted.
 *
 * [commitid ID;]
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_commitid(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_COMMITID) != RCS_TYPE_COMMITID)
		return (1);

	pdp->rp_delta->rd_commitid = pdp->rp_value.str;

	return (rcsparse_token(rfp, RCS_TOK_SCOLON) != RCS_TOK_SCOLON);
}

/*
 * rcsparse_textrevision()
 *
 * Called upon reaching a new REVISION entry in the delta text section.
 * pdp->rp_delta will be set to REVISION's delta (created in delta section)
 * for further parsing.
 *
 * REVISION
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_textrevision(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	struct rcs_delta *rdp;

	TAILQ_FOREACH(rdp, &rfp->rf_delta, rd_list)
	{
		if (rcsnum_cmp(rdp->rd_num, pdp->rp_value.rev, 0) == 0)
			break;
	}
	if (rdp == NULL) {
		rcsparse_warnx(
		    rfp, "delta for revision \"%s\" not found", pdp->rp_buf);
		free(pdp->rp_value.rev);
		return (1);
	}
	if (rdp->rd_text != NULL) {
		rcsparse_warnx(rfp, "duplicate deltatext for revision \"%s\"",
		    pdp->rp_buf);
		free(pdp->rp_value.rev);
		return (1);
	}
	pdp->rp_delta = rdp;

	free(pdp->rp_value.rev);
	return (0);
}

/*
 * rcsparse_log()
 *
 * Parses the specified log of current deltatext pdp->rp_delta.
 *
 * log @[...]@
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_log(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_STRING) != RCS_TYPE_STRING)
		return (1);

	pdp->rp_delta->rd_log = pdp->rp_value.str;

	return (0);
}

/*
 * rcsparse_text()
 *
 * Parses the specified text of current deltatext pdp->rp_delta.
 *
 * text @[...]@
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_text(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	if (rcsparse_token(rfp, RCS_TYPE_STRING) != RCS_TYPE_STRING)
		return (1);

	pdp->rp_delta->rd_tlen = pdp->rp_tlen - 1;
	if (pdp->rp_delta->rd_tlen == 0) {
		pdp->rp_delta->rd_text = (u_char *)xstrdup("");
	} else {
		pdp->rp_delta->rd_text = xmalloc(pdp->rp_delta->rd_tlen);
		memcpy(pdp->rp_delta->rd_text, pdp->rp_buf,
		    pdp->rp_delta->rd_tlen);
	}
	free(pdp->rp_value.str);

	return (0);
}

/*
 * rcsparse_head()
 *
 * Parses the head revision of RCS file <rfp>.
 *
 * head [REVISION];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_head(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	int type;

	type = rcsparse_token(rfp, RCS_TYPE_REVISION | RCS_TOK_SCOLON);
	if (type == RCS_TYPE_REVISION) {
		rfp->rf_head = pdp->rp_value.rev;
		type = rcsparse_token(rfp, RCS_TOK_SCOLON);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_branch()
 *
 * Parses the default branch of RCS file <rfp>. The branch keyword is
 * optional and can be omitted.
 *
 * [branch BRANCH;]
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_branch(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	int type;

	type = rcsparse_token(rfp, RCS_TYPE_BRANCH | RCS_TOK_SCOLON);
	if (type == RCS_TYPE_BRANCH) {
		rfp->rf_branch = pdp->rp_value.rev;
		type = rcsparse_token(rfp, RCS_TOK_SCOLON);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_access()
 *
 * Parses the access list of RCS file <rfp>.
 *
 * access [LOGIN ...];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_access(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	struct rcs_access *ap;
	int		   type;

	while ((type = rcsparse_token(rfp, RCS_TOK_SCOLON | RCS_TYPE_LOGIN)) ==
	    RCS_TYPE_LOGIN) {
		ap = xmalloc(sizeof(*ap));
		ap->ra_name = pdp->rp_value.str;
		TAILQ_INSERT_TAIL(&(rfp->rf_access), ap, ra_list);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_symbols()
 *
 * Parses the symbol list of RCS file <rfp>.
 *
 * symbols [SYMBOL:REVISION ...];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_symbols(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	struct rcs_sym *symp;
	char	       *name;
	int		type;

	while ((type = rcsparse_token(rfp, RCS_TOK_SCOLON | RCS_TYPE_SYMBOL)) ==
	    RCS_TYPE_SYMBOL) {
		name = pdp->rp_value.str;
		if (rcsparse_token(rfp, RCS_TOK_COLON) != RCS_TOK_COLON ||
		    rcsparse_token(rfp, RCS_TYPE_NUMBER) != RCS_TYPE_NUMBER) {
			free(name);
			return (1);
		}
		symp = xmalloc(sizeof(*symp));
		symp->rs_name = name;
		symp->rs_num = pdp->rp_value.rev;
		TAILQ_INSERT_TAIL(&(rfp->rf_symbols), symp, rs_list);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_locks()
 *
 * Parses the lock list of RCS file <rfp>.
 *
 * locks [SYMBOL:REVISION ...];
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_locks(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	struct rcs_lock *lkp;
	char		*name;
	int		 type;

	while ((type = rcsparse_token(rfp, RCS_TOK_SCOLON | RCS_TYPE_LOGIN)) ==
	    RCS_TYPE_LOGIN) {
		name = pdp->rp_value.str;
		if (rcsparse_token(rfp, RCS_TOK_COLON) != RCS_TOK_COLON ||
		    rcsparse_token(rfp, RCS_TYPE_REVISION) !=
			RCS_TYPE_REVISION) {
			free(name);
			return (1);
		}
		lkp = xmalloc(sizeof(*lkp));
		lkp->rl_name = name;
		lkp->rl_num = pdp->rp_value.rev;
		TAILQ_INSERT_TAIL(&(rfp->rf_locks), lkp, rl_list);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_locks()
 *
 * Parses the strict keyword of RCS file <rfp>. The strict keyword is
 * optional and can be omitted.
 *
 * [strict;]
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_strict(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	(void)pdp;
	rfp->rf_flags |= RCS_SLOCK;

	return (rcsparse_token(rfp, RCS_TOK_SCOLON) != RCS_TOK_SCOLON);
}

/*
 * rcsparse_comment()
 *
 * Parses the comment of RCS file <rfp>.  The comment keyword is optional
 * and can be omitted.
 *
 * [comment [@[...]@];]
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_comment(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	int type;

	type = rcsparse_token(rfp, RCS_TYPE_STRING | RCS_TOK_SCOLON);
	if (type == RCS_TYPE_STRING) {
		rfp->rf_comment = pdp->rp_value.str;
		type = rcsparse_token(rfp, RCS_TOK_SCOLON);
	}

	return (type != RCS_TOK_SCOLON);
}

/*
 * rcsparse_expand()
 *
 * Parses expand of RCS file <rfp>.  The expand keyword is optional and
 * can be omitted.
 *
 * [expand [@[...]@];]
 *
 * Returns 0 on success or 1 on failure.
 */
static int
rcsparse_expand(RCSFILE *rfp, struct rcs_pdata *pdp)
{
	int type;

	type = rcsparse_token(rfp, RCS_TYPE_STRING | RCS_TOK_SCOLON);
	if (type == RCS_TYPE_STRING) {
		rfp->rf_expand = pdp->rp_value.str;
		type = rcsparse_token(rfp, RCS_TOK_SCOLON);
	}

	return (type != RCS_TOK_SCOLON);
}

#define RBUF_PUTC(ch)                                                          \
	do {                                                                   \
		if (bp == pdp->rp_bufend - 1) {                                \
			len = bp - pdp->rp_buf;                                \
			rcsparse_growbuf(rfp);                                 \
			bp = pdp->rp_buf + len;                                \
		}                                                              \
		*(bp++) = (char)(unsigned char)(ch);                           \
		pdp->rp_tlen++;                                                \
	} while (0);

/*
 * The lexer enters after the opening delimiter. A doubled at sign is
 * one literal byte, while a lone at sign ends the string.
 */
static int
rcsparse_string(RCSFILE *rfp, int allowed)
{
	struct rcs_pdata *pdp;
	int		  c;
	size_t		  len;
	char		 *bp;

	pdp = (struct rcs_pdata *)rfp->rf_pdata;

	bp = pdp->rp_buf;
	pdp->rp_tlen = 0;
	*bp = '\0';

	for (;;) {
		c = getc(rfp->rf_file);
		if (c == '@') {
			c = getc(rfp->rf_file);
			if (c == EOF) {
				return (EOF);
			} else if (c != '@') {
				ungetc(c, rfp->rf_file);
				break;
			}
		}

		if (c == EOF) {
			return (EOF);
		} else if (c == '\n')
			pdp->rp_lineno++;

		RBUF_PUTC(c);
	}

	bp = pdp->rp_buf + pdp->rp_tlen;
	RBUF_PUTC('\0');

	if (!(allowed & RCS_TYPE_STRING)) {
		rcsparse_warnx(rfp, "unexpected RCS string");
		return (0);
	}

	pdp->rp_value.str = xstrdup(pdp->rp_buf);

	return (RCS_TYPE_STRING);
}

static int
rcsparse_token(RCSFILE *rfp, int allowed)
{
	const struct rcs_keyword *p;
	struct rcs_pdata	 *pdp;
	int			  c, pre, ret, type, year, maximum_day;
	char			 *bp;
	size_t			  len;
	RCSNUM			 *datenum;

	pdp = (struct rcs_pdata *)rfp->rf_pdata;

	if (pdp->rp_token != -1) {
		/* no need to check for allowed here */
		type = pdp->rp_token;
		pdp->rp_token = -1;
		return (type);
	}

	/* skip whitespaces */
	c = EOF;
	do {
		pre = c;
		c = getc(rfp->rf_file);
		if (c == EOF) {
			if (ferror(rfp->rf_file)) {
				rcsparse_warnx(rfp, "error during parsing");
				return (0);
			}
			if (pre != '\n')
				rcsparse_warnx(
				    rfp, "no newline at end of file");
			return (EOF);
		} else if (c == '\n')
			pdp->rp_lineno++;
	} while (isspace(c));

	pdp->rp_msglineno = pdp->rp_lineno;
	type = 0;
	switch (c) {
	case '@':
		ret = rcsparse_string(rfp, allowed);
		if (ret == EOF && ferror(rfp->rf_file)) {
			rcsparse_warnx(rfp, "error during parsing");
			return (0);
		}
		return (ret);
		/* NOTREACHED */
	case ':':
		type = RCS_TOK_COLON;
		if (type & allowed)
			return (type);
		rcsparse_warnx(rfp, "unexpected token \"%c\"", c);
		return (0);
		/* NOTREACHED */
	case ';':
		type = RCS_TOK_SCOLON;
		if (type & allowed)
			return (type);
		rcsparse_warnx(rfp, "unexpected token \"%c\"", c);
		return (0);
		/* NOTREACHED */
	case ',':
		type = RCS_TOK_COMMA;
		if (type & allowed)
			return (type);
		rcsparse_warnx(rfp, "unexpected token \"%c\"", c);
		return (0);
		/* NOTREACHED */
	default:
		if (!isgraph(c)) {
			rcsparse_warnx(rfp, "unexpected character 0x%.2X", c);
			return (0);
		}
		break;
	}
	allowed &= ~(RCS_TOK_COLON | RCS_TOK_SCOLON | RCS_TOK_COMMA);

	bp = pdp->rp_buf;
	pdp->rp_tlen = 0;
	*bp = '\0';

	for (;;) {
		if (c == EOF) {
			if (ferror(rfp->rf_file))
				rcsparse_warnx(rfp, "error during parsing");
			else
				rcsparse_warnx(rfp, "unexpected end of file");
			return (0);
		} else if (c == '\n')
			pdp->rp_lineno++;

		RBUF_PUTC(c);

		c = getc(rfp->rf_file);

		if (isspace(c)) {
			if (c == '\n')
				pdp->rp_lineno++;
			RBUF_PUTC('\0');
			break;
		} else if (c == ';' || c == ':' || c == ',') {
			ungetc(c, rfp->rf_file);
			RBUF_PUTC('\0');
			break;
		} else if (!isgraph(c)) {
			rcsparse_warnx(rfp, "unexpected character 0x%.2X", c);
			return (0);
		}
	}

	switch (allowed) {
	case RCS_TYPE_COMMITID:
		if (!valid_commitid(pdp->rp_buf)) {
			rcsparse_warnx(
			    rfp, "invalid commitid \"%s\"", pdp->rp_buf);
			return (0);
		}
		pdp->rp_value.str = xstrdup(pdp->rp_buf);
		break;
	case RCS_TYPE_LOGIN:
		if (!valid_login(pdp->rp_buf)) {
			rcsparse_warnx(
			    rfp, "invalid login \"%s\"", pdp->rp_buf);
			return (0);
		}
		pdp->rp_value.str = xstrdup(pdp->rp_buf);
		break;
	case RCS_TYPE_SYMBOL:
		if (!rcs_sym_check(pdp->rp_buf)) {
			rcsparse_warnx(
			    rfp, "invalid symbol \"%s\"", pdp->rp_buf);
			return (0);
		}
		pdp->rp_value.str = xstrdup(pdp->rp_buf);
		break;
		/* FALLTHROUGH */
	case RCS_TYPE_STATE:
		if (rcs_state_check(pdp->rp_buf)) {
			rcsparse_warnx(
			    rfp, "invalid state \"%s\"", pdp->rp_buf);
			return (0);
		}
		pdp->rp_value.str = xstrdup(pdp->rp_buf);
		break;
	case RCS_TYPE_DATE:
		if ((datenum = rcsnum_parse(pdp->rp_buf)) == NULL) {
			rcsparse_warnx(rfp, "invalid date \"%s\"", pdp->rp_buf);
			return (0);
		}
		if (datenum->rn_len != 6 || datenum->rn_id[1] < 1 ||
		    datenum->rn_id[1] > 12 || datenum->rn_id[2] < 1 ||
		    datenum->rn_id[2] > 31 || datenum->rn_id[3] > 23 ||
		    datenum->rn_id[4] > 59 || datenum->rn_id[5] > 60) {
			free(datenum);
			rcsparse_warnx(rfp, "invalid date \"%s\"", pdp->rp_buf);
			return (0);
		}
		year = datenum->rn_id[0];
		if (year < 1900)
			year += 1900;
		maximum_day = 31;
		if (datenum->rn_id[1] == 4 || datenum->rn_id[1] == 6 ||
		    datenum->rn_id[1] == 9 || datenum->rn_id[1] == 11)
			maximum_day = 30;
		else if (datenum->rn_id[1] == 2)
			maximum_day = 28 +
			    (year % 4 == 0 &&
				(year % 100 != 0 || year % 400 == 0));
		if (datenum->rn_id[2] > maximum_day) {
			free(datenum);
			rcsparse_warnx(
			    rfp, "invalid calendar date \"%s\"", pdp->rp_buf);
			return (0);
		}
		memset(&pdp->rp_value.date, 0, sizeof(pdp->rp_value.date));
		pdp->rp_value.date.tm_year = datenum->rn_id[0];
		if (pdp->rp_value.date.tm_year >= 1900)
			pdp->rp_value.date.tm_year -= 1900;
		pdp->rp_value.date.tm_mon = datenum->rn_id[1] - 1;
		pdp->rp_value.date.tm_mday = datenum->rn_id[2];
		pdp->rp_value.date.tm_hour = datenum->rn_id[3];
		pdp->rp_value.date.tm_min = datenum->rn_id[4];
		pdp->rp_value.date.tm_sec = datenum->rn_id[5];
		free(datenum);
		break;
	case RCS_TYPE_NUMBER:
		pdp->rp_value.rev = rcsnum_parse(pdp->rp_buf);
		if (pdp->rp_value.rev == NULL) {
			rcsparse_warnx(
			    rfp, "invalid number \"%s\"", pdp->rp_buf);
			return (0);
		}
		break;
	case RCS_TYPE_BRANCH:
		pdp->rp_value.rev = rcsnum_parse(pdp->rp_buf);
		if (pdp->rp_value.rev == NULL) {
			rcsparse_warnx(
			    rfp, "invalid branch \"%s\"", pdp->rp_buf);
			return (0);
		}
		if (!RCSNUM_ISBRANCH(pdp->rp_value.rev)) {
			free(pdp->rp_value.rev);
			rcsparse_warnx(
			    rfp, "expected branch, got \"%s\"", pdp->rp_buf);
			return (0);
		}
		break;
	case RCS_TYPE_KEYWORD:
		if (islower(*pdp->rp_buf)) {
			p = bsearch(pdp->rp_buf, keywords,
			    sizeof(keywords) / sizeof(keywords[0]),
			    sizeof(keywords[0]), kw_cmp);
			if (p != NULL)
				return (p->k_val);
		}
		allowed = RCS_TYPE_REVISION;
		/* FALLTHROUGH */
	case RCS_TYPE_REVISION:
		pdp->rp_value.rev = rcsnum_parse(pdp->rp_buf);
		if (pdp->rp_value.rev != NULL) {
			if (RCSNUM_ISBRANCH(pdp->rp_value.rev)) {
				free(pdp->rp_value.rev);
				rcsparse_warnx(rfp,
				    "expected revision, got \"%s\"",
				    pdp->rp_buf);
				return (0);
			}
			break;
		}
		/* FALLTHROUGH */
	default:
		RBUF_PUTC('\0');
		rcsparse_warnx(rfp, "unexpected token \"%s\"", pdp->rp_buf);
		return (0);
		/* NOTREACHED */
	}

	return (allowed);
}

static int
rcsparse(RCSFILE *rfp, struct rcs_section *sec)
{
	struct rcs_pdata *pdp;
	int		  i, token;

	pdp = (struct rcs_pdata *)rfp->rf_pdata;
	i = 0;

	token = 0;
	for (i = 0; sec[i].token != 0; i++) {
		token = rcsparse_token(rfp, RCS_TYPE_KEYWORD);
		if (token == 0)
			return (1);

		while (token != sec[i].token) {
			if (sec[i].parse == NULL)
				goto end;
			if (sec[i].opt) {
				i++;
				continue;
			}
			if (i == 0 &&
			    ((sec == sec_delta && token == RCS_TOK_DESC) ||
				(sec == sec_deltatext && token == EOF)))
				goto end;
			rcsparse_warnx(
			    rfp, "unexpected token \"%s\"", pdp->rp_buf);
			return (1);
		}

		if (sec[i].parse(rfp, pdp))
			return (1);
	}
end:
	if (token == RCS_TYPE_REVISION)
		pdp->rp_token = token;
	else if (token == RCS_TOK_DESC)
		pdp->rp_token = RCS_TOK_DESC;
	else if (token == EOF)
		rfp->rf_flags |= RCS_PARSED;

	return (0);
}

static int
rcsparse_deltatext(RCSFILE *rfp)
{
	int ret;

	if (rfp->rf_flags & PARSED_DELTATEXTS)
		return (0);

	if (!(rfp->rf_flags & PARSED_DESC))
		if ((ret = rcsparse_desc(rfp)))
			return (ret);

	if (rcsparse(rfp, sec_deltatext))
		return (-1);

	return ((rfp->rf_flags & RCS_PARSED) ? 0 : 1);
}

static int
rcsparse_delta(RCSFILE *rfp)
{
	struct rcs_pdata *pdp;

	if (rfp->rf_flags & PARSED_DELTAS)
		return (0);

	pdp = (struct rcs_pdata *)rfp->rf_pdata;
	if (pdp->rp_token == RCS_TOK_DESC)
		return (0);

	if (rcsparse(rfp, sec_delta))
		return (-1);

	if (pdp->rp_delta != NULL) {
		TAILQ_INSERT_TAIL(&rfp->rf_delta, pdp->rp_delta, rd_list);
		pdp->rp_delta = NULL;
		rfp->rf_ndelta++;
		return (1);
	}

	return (0);
}

/*
 * rcsparse_growbuf()
 *
 * Attempt to grow the internal parse buffer for the RCS file <rf> by
 * RCS_BUFEXTSIZE.
 * In case of failure, the original buffer is left unmodified.
 */
static void
rcsparse_growbuf(RCSFILE *rfp)
{
	struct rcs_pdata *pdp = (struct rcs_pdata *)rfp->rf_pdata;

	if (pdp->rp_blen > SIZE_MAX - RCS_BUFEXTSIZE)
		fatal("RCS parse buffer overflow");
	pdp->rp_buf =
	    xreallocarray(pdp->rp_buf, 1, pdp->rp_blen + RCS_BUFEXTSIZE);
	pdp->rp_blen += RCS_BUFEXTSIZE;
	pdp->rp_bufend = pdp->rp_buf + pdp->rp_blen - 1;
}

/*
 * Borrowed from src/usr.sbin/user/user.c:
 * return 1 if `login' is a valid login name
 */
static int
valid_login(char *login_name)
{
	unsigned char *cp;

	/* The first character cannot be a hyphen */
	if (*login_name == '-')
		return 0;

	for (cp = (unsigned char *)login_name; *cp; cp++) {
		/* We allow '$' as the last character for samba */
		if (!isalnum(*cp) && *cp != '.' && *cp != '_' && *cp != '-' &&
		    !(*cp == '$' && *(cp + 1) == '\0')) {
			return 0;
		}
	}
	/* Preserve OpenBSD's historical RCS login limit, not host account
	 * policy. */
	if ((char *)cp - login_name > 31)
		return 0;
	return 1;
}

static int
valid_commitid(char *commitid)
{
	unsigned char *cp;

	/* A-Za-z0-9 */
	for (cp = (unsigned char *)commitid; *cp; cp++) {
		if (!isalnum(*cp))
			return 0;
	}
	if ((char *)cp - commitid > RCS_COMMITID_MAXLEN)
		return 0;
	return 1;
}

static int
kw_cmp(const void *k, const void *e)
{
	return (strcmp(k, ((const struct rcs_keyword *)e)->k_name));
}

static void
rcsparse_warnx(RCSFILE *rfp, char *fmt, ...)
{
	struct rcs_pdata *pdp;
	va_list		  ap;
	char		 *msg;

	pdp = (struct rcs_pdata *)rfp->rf_pdata;
	va_start(ap, fmt);
	if (vasprintf(&msg, fmt, ap) == -1) {
		cvs_log(LP_ERRNO, "vasprintf");
		va_end(ap);
		return;
	}
	va_end(ap);
	cvs_log(LP_ERR, "%s:%d: %s", rfp->rf_path, pdp->rp_msglineno, msg);
	free(msg);
}
