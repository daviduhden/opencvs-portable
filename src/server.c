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
/*	$OpenBSD: server.c,v 1.106 2020/10/19 19:51:20 naddy Exp $	*/
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
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

struct cvs_resp cvs_responses[] = {
    /* this is what our server uses, the client should support it */
    {"Valid-requests", 1, cvs_client_validreq, RESP_NEEDED},
    {"ok", 0, cvs_client_ok, RESP_NEEDED},
    {"error", 0, cvs_client_error, RESP_NEEDED},
    {"E", 0, cvs_client_e, RESP_NEEDED}, {"M", 0, cvs_client_m, RESP_NEEDED},
    {"Checked-in", 0, cvs_client_checkedin, RESP_NEEDED},
    {"Updated", 0, cvs_client_updated, RESP_NEEDED},
    {"Merged", 0, cvs_client_merged, RESP_NEEDED},
    {"Removed", 0, cvs_client_removed, RESP_NEEDED},
    {"Remove-entry", 0, cvs_client_remove_entry, 0},
    {"Set-static-directory", 0, cvs_client_set_static_directory, 0},
    {"Clear-static-directory", 0, cvs_client_clear_static_directory, 0},
    {"Set-sticky", 0, cvs_client_set_sticky, 0},
    {"Clear-sticky", 0, cvs_client_clear_sticky, 0},

    /* unsupported responses until told otherwise */
    {"New-entry", 0, NULL, 0}, {"Created", 0, NULL, 0},
    {"Update-existing", 0, NULL, 0}, {"Rcs-diff", 0, NULL, 0},
    {"Patched", 0, NULL, 0}, {"Mode", 0, NULL, 0}, {"Mod-time", 0, NULL, 0},
    {"Checksum", 0, NULL, 0}, {"Copy-file", 0, NULL, 0},
    {"Template", 0, NULL, 0}, {"Set-checkin-prog", 0, NULL, 0},
    {"Set-update-prog", 0, NULL, 0}, {"Notified", 0, NULL, 0},
    {"Module-expansion", 0, NULL, 0}, {"Wrapper-rcsOption", 0, NULL, 0},
    {"Mbinary", 0, NULL, 0}, {"F", 0, NULL, 0}, {"MT", 0, NULL, 0},
    {"", -1, NULL, 0}};

int   cvs_server(int, char **);
char *cvs_server_path = NULL;

static char	  *server_currentdir = NULL;
static char	 **server_argv;
static int	   server_argc = 1;
static const char *server_allowed_root;

extern int disable_fast_checkout;

struct cvs_cmd cvs_cmd_server = {CVS_OP_SERVER, CVS_USE_WDIR, "server",
    {"", ""}, "server mode", NULL, NULL, NULL, cvs_server};

int
cvs_server(int argc, char **argv)
{
	(void)argv;
	char	       *cmd, *data, *temporary;
	struct cvs_req *req;
	size_t		requests = 0;

	if (argc > 1)
		fatal("server does not take any extra arguments");
	server_allowed_root = getenv("OPENCVS_SERVER_ROOT");
	if (server_allowed_root != NULL &&
	    (!pc_native_absolute(server_allowed_root) ||
		strpbrk(server_allowed_root, "\r\n") != NULL))
		fatal(
		    "OPENCVS_SERVER_ROOT must be an absolute repository path");

	/* Be on server-side very verbose per default. */
	verbosity = 2;

	if (setvbuf(stdin, NULL, _IONBF, 0) != 0 ||
	    setvbuf(stdout, NULL, _IONBF, 0) != 0)
		fatal("cannot configure server protocol streams");

	cvs_server_active = 1;

	server_argv = xcalloc(server_argc + 1, sizeof(*server_argv));
	server_argv[0] = xstrdup("server");

	(void)xasprintf(&temporary, "%s/cvs-serv%d", cvs_tmpdir, getpid());

	if (mkdir(temporary, 0700) == -1)
		fatal("failed to create temporary server directory: %s, %s",
		    temporary, strerror(errno));
	cvs_server_path = temporary;

	if (chdir(cvs_server_path) == -1)
		fatal("failed to change directory to '%s'", cvs_server_path);

	for (;;) {
		cmd = cvs_remote_input();
		if (++requests > 1000000)
			fatal("server session request limit exceeded");

		if ((data = strchr(cmd, ' ')) != NULL)
			(*data++) = '\0';

		req = cvs_remote_get_request_info(cmd);
		if (req == NULL)
			fatal(
			    "request '%s' is not supported by our server", cmd);

		if (req->hdlr == NULL)
			fatal("opencvs server does not support '%s'", cmd);

		if ((req->flags & REQ_NEEDDIR) && (server_currentdir == NULL))
			fatal("`%s' needs a directory to be sent with "
			      "the `Directory` request first",
			    cmd);

		(*req->hdlr)(data);
		free(cmd);
	}
}

void
cvs_server_send_response(char *fmt, ...)
{
	int	i;
	va_list ap;
	char   *data;

	va_start(ap, fmt);
	i = vasprintf(&data, fmt, ap);
	va_end(ap);
	if (i == -1)
		fatal("cvs_server_send_response: could not allocate memory");

	cvs_log(LP_TRACE, "%s", data);
	cvs_remote_output(data);
	free(data);
}

void
cvs_server_root(char *data)
{
	if (data == NULL)
		fatal("Missing argument for Root");

	if (current_cvsroot != NULL)
		return;

	if (server_allowed_root != NULL &&
	    strcmp(data, server_allowed_root) != 0)
		fatal("repository root is not authorized for this server");
	if (!pc_native_absolute(data) ||
	    (current_cvsroot = cvsroot_get(data)) == NULL)
		fatal("Invalid Root specified!");
	if (!cvsroot_is_local() ||
	    (server_allowed_root != NULL &&
		strcmp(current_cvsroot->cr_dir, server_allowed_root) != 0))
		fatal("repository root is not authorized for this server");

	cvs_parse_configfile();
	cvs_parse_modules();
	umask((mode_t)cvs_umask);
}

void
cvs_server_validresp(char *data)
{
	int		 i;
	char		*sp, *ep;
	struct cvs_resp *resp;

	if ((sp = data) == NULL)
		fatal("Missing argument for Valid-responses");

	do {
		if ((ep = strchr(sp, ' ')) != NULL)
			*ep = '\0';

		resp = cvs_remote_get_response_info(sp);
		if (resp != NULL)
			resp->supported = 1;

		if (ep != NULL)
			sp = ep + 1;
	} while (ep != NULL);

	for (i = 0; cvs_responses[i].supported != -1; i++) {
		resp = &cvs_responses[i];
		if ((resp->flags & RESP_NEEDED) && resp->supported != 1) {
			fatal("client does not support required '%s'",
			    resp->name);
		}
	}
}

void
cvs_server_validreq(char *data)
{
	(void)data;
	BUF  *bp;
	char *d;
	int   i, first;

	first = 0;
	bp = buf_alloc(512);
	for (i = 0; cvs_requests[i].supported != -1; i++) {
		if (cvs_requests[i].hdlr == NULL)
			continue;

		if (first != 0)
			buf_putc(bp, ' ');
		else
			first++;

		buf_puts(bp, cvs_requests[i].name);
	}

	buf_putc(bp, '\0');
	d = buf_release(bp);

	cvs_server_send_response("Valid-requests %s", d);
	cvs_server_send_response("ok");
	free(d);
}

void
cvs_server_static_directory(char *data)
{
	(void)data;
	FILE *fp;
	char  fpath[PATH_MAX];

	(void)xsnprintf(fpath, PATH_MAX, "%s/%s", server_currentdir,
	    CVS_PATH_STATICENTRIES);

	if ((fp = fopen(fpath, "w+")) == NULL) {
		cvs_log(LP_ERRNO, "%s", fpath);
		return;
	}
	(void)fclose(fp);
}

void
cvs_server_sticky(char *data)
{
	FILE *fp;
	char  tagpath[PATH_MAX];

	if (data == NULL)
		fatal("Missing argument for Sticky");

	(void)xsnprintf(
	    tagpath, PATH_MAX, "%s/%s", server_currentdir, CVS_PATH_TAG);

	if ((fp = fopen(tagpath, "w+")) == NULL) {
		cvs_log(LP_ERRNO, "%s", tagpath);
		return;
	}

	(void)fprintf(fp, "%s\n", data);
	(void)fclose(fp);
}

void
cvs_server_globalopt(char *data)
{
	if (data == NULL)
		fatal("Missing argument for Global_option");

	if (!strcmp(data, "-l"))
		cvs_nolog = 1;

	if (!strcmp(data, "-n"))
		cvs_noexec = 1;

	if (!strcmp(data, "-Q"))
		verbosity = 0;

	if (!strcmp(data, "-q"))
		verbosity = 1;

	if (!strcmp(data, "-r"))
		cvs_readonly = 1;

	if (!strcmp(data, "-t"))
		cvs_trace = 1;
}

void
cvs_server_set(char *data)
{
	char *ep;

	if (data == NULL)
		fatal("Missing argument for Set");

	ep = strchr(data, '=');
	if (ep == NULL)
		fatal("no = in variable assignment");

	*(ep++) = '\0';
	if (cvs_var_set(data, ep) < 0)
		fatal("cvs_server_set: cvs_var_set failed");
}

void
cvs_server_directory(char *data)
{
	(void)data;
	CVSENTRIES *entlist;
	char	   *dir, *repo, *parent, *entry, *dirn, *p;
	char	    parentbuf[PATH_MAX], dirnbuf[PATH_MAX];

	if (current_cvsroot == NULL)
		fatal("No Root specified for Directory");

	dir = cvs_remote_input();
	STRIP_SLASH(dir);

	if (strlen(dir) < strlen(current_cvsroot->cr_dir) ||
	    strncmp(dir, current_cvsroot->cr_dir,
		strlen(current_cvsroot->cr_dir)) != 0 ||
	    (current_cvsroot->cr_dir[strlen(current_cvsroot->cr_dir) - 1] !=
		    '/' &&
		dir[strlen(current_cvsroot->cr_dir)] != '\0' &&
		dir[strlen(current_cvsroot->cr_dir)] != '/'))
		fatal("cvs_server_directory: bad Directory request");

	repo = dir + strlen(current_cvsroot->cr_dir);

	/*
	 * This is somewhat required for checkout, as the
	 * directory request will be:
	 *
	 * Directory .
	 * /path/to/cvs/root
	 */
	if (repo[0] == '\0')
		p = xstrdup(".");
	else
		p = xstrdup(*repo == '/' ? repo + 1 : repo);

	if (pc_any_absolute(p))
		fatal("absolute path in Directory request");
	cvs_validate_directory(p);
	cvs_mkpath(p, NULL);

	if (strlcpy(dirnbuf, p, sizeof(dirnbuf)) >= sizeof(dirnbuf))
		fatal("cvs_server_directory: truncation");
	if ((dirn = basename(dirnbuf)) == NULL)
		fatal("cvs_server_directory: %s", strerror(errno));

	if (strlcpy(parentbuf, p, sizeof(parentbuf)) >= sizeof(parentbuf))
		fatal("cvs_server_directory: truncation");
	if ((parent = dirname(parentbuf)) == NULL)
		fatal("cvs_server_directory: %s", strerror(errno));

	if (strcmp(parent, ".")) {
		entry = xmalloc(CVS_ENT_MAXLINELEN);
		cvs_ent_line_str(dirn, NULL, NULL, NULL, NULL, 1, 0, entry,
		    CVS_ENT_MAXLINELEN);

		entlist = cvs_ent_open(parent);
		cvs_ent_add(entlist, entry);
		free(entry);
	}

	free(server_currentdir);
	server_currentdir = p;

	free(dir);
}

void
cvs_server_entry(char *data)
{
	CVSENTRIES *entlist;

	if (data == NULL)
		fatal("Missing argument for Entry");

	entlist = cvs_ent_open(server_currentdir);
	cvs_ent_add(entlist, data);
}

void
cvs_server_modified(char *data)
{
	int	    fd;
	size_t	    flen;
	mode_t	    fmode;
	const char *errstr;
	char	   *mode, *len, fpath[PATH_MAX];

	if (data == NULL)
		fatal("Missing argument for Modified");
	if (*data == '\0' || !strcmp(data, ".") || !strcmp(data, "..") ||
	    strpbrk(data, "/\\:\r\n") != NULL)
		fatal("invalid filename in Modified request");
	if (server_currentdir == NULL)
		fatal("Directory must precede Modified");

	/* sorry, we have to use TMP_DIR */
	disable_fast_checkout = 1;

	mode = cvs_remote_input();
	len = cvs_remote_input();

	cvs_strtomode(mode, &fmode);
	free(mode);

	flen = strtonum(len, 0, INT_MAX, &errstr);
	if (errstr != NULL)
		fatal("cvs_server_modified: %s", errstr);
	free(len);

	(void)xsnprintf(fpath, PATH_MAX, "%s/%s", server_currentdir, data);

	if ((fd = open(
		 fpath, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600)) == -1)
		fatal("cvs_server_modified: %s: %s", fpath, strerror(errno));

	cvs_remote_receive_file(fd, flen);

	if (fchmod(fd, fmode & (S_IRWXU | S_IRWXG | S_IRWXO)) == -1)
		fatal("cvs_server_modified: failed to set file mode");

	if (close(fd) == -1)
		fatal("cannot close uploaded file: %s", strerror(errno));
}

void
cvs_server_useunchanged(char *data)
{
	(void)data;
}

void
cvs_server_unchanged(char *data)
{
	char		fpath[PATH_MAX];
	CVSENTRIES     *entlist;
	struct cvs_ent *ent;
	char		sticky[CVS_ENT_MAXLINELEN];
	char		rev[CVS_REV_BUFSZ], entry[CVS_ENT_MAXLINELEN];

	if (data == NULL)
		fatal("Missing argument for Unchanged");

	/* sorry, we have to use TMP_DIR */
	disable_fast_checkout = 1;

	(void)xsnprintf(fpath, PATH_MAX, "%s/%s", server_currentdir, data);

	entlist = cvs_ent_open(server_currentdir);
	ent = cvs_ent_get(entlist, data);
	if (ent == NULL)
		fatal("received Unchanged request for non-existing file");

	sticky[0] = '\0';
	if (ent->ce_tag != NULL)
		(void)xsnprintf(sticky, sizeof(sticky), "T%s", ent->ce_tag);

	rcsnum_tostr(ent->ce_rev, rev, sizeof(rev));
	(void)xsnprintf(entry, sizeof(entry), "/%s/%s/%s/%s/%s", ent->ce_name,
	    rev, CVS_SERVER_UNCHANGED, ent->ce_opts ? ent->ce_opts : "",
	    sticky);

	cvs_ent_free(ent);
	cvs_ent_add(entlist, entry);
}

void
cvs_server_questionable(char *data)
{
	CVSENTRIES *entlist;
	char	    entry[CVS_ENT_MAXLINELEN];

	if (data == NULL)
		fatal("Questionable request with no data attached");

	(void)xsnprintf(
	    entry, sizeof(entry), "/%s/%c///", data, CVS_SERVER_QUESTIONABLE);

	entlist = cvs_ent_open(server_currentdir);
	cvs_ent_add(entlist, entry);

	/* sorry, we have to use TMP_DIR */
	disable_fast_checkout = 1;
}

void
cvs_server_argument(char *data)
{
	if (data == NULL)
		fatal("Missing argument for Argument");

	if (server_argc > INT_MAX - 2)
		fatal("too many protocol arguments");
	server_argv = xreallocarray(
	    server_argv, (size_t)server_argc + 2, sizeof(*server_argv));
	server_argv[server_argc] = xstrdup(data);
	server_argv[++server_argc] = NULL;
}

void
cvs_server_argumentx(char *data)
{
	int    idx;
	size_t len, oldlen, extra;

	if (data == NULL)
		fatal("Missing argument for ArgumentX");
	if (server_argc == 1)
		fatal("Protocol Error: ArgumentX without previous argument");

	idx = server_argc - 1;

	oldlen = strlen(server_argv[idx]);
	extra = strlen(data);
	if (oldlen > SIZE_MAX - 2 || extra > SIZE_MAX - oldlen - 2)
		fatal("protocol argument size overflow");
	len = oldlen + extra + 2;
	server_argv[idx] = xreallocarray(server_argv[idx], len, sizeof(char));
	strlcat(server_argv[idx], "\n", len);
	strlcat(server_argv[idx], data, len);
}

void
cvs_server_update_patches(char *data)
{
	(void)data;
	/*
	 * This does not actually do anything.
	 * It is used to tell that the server is able to
	 * generate patches when given an `update' request.
	 * The client must issue the -u argument to `update'
	 * to receive patches.
	 */
}

void
cvs_server_add(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_add: %s", strerror(errno));

	cvs_cmdop = CVS_OP_ADD;
	cmdp->cmd_flags = cvs_cmd_add.cmd_flags;
	cvs_add(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_import(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_import: %s", strerror(errno));

	cvs_cmdop = CVS_OP_IMPORT;
	cmdp->cmd_flags = cvs_cmd_import.cmd_flags;
	if (cvs_import(server_argc, server_argv) != 0)
		cvs_server_send_response("error  import failed");
	else
		cvs_server_send_response("ok");
}

void
cvs_server_admin(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_admin: %s", strerror(errno));

	cvs_cmdop = CVS_OP_ADMIN;
	cmdp->cmd_flags = cvs_cmd_admin.cmd_flags;
	cvs_admin(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_annotate(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_annotate: %s", strerror(errno));

	cvs_cmdop = CVS_OP_ANNOTATE;
	cmdp->cmd_flags = cvs_cmd_annotate.cmd_flags;
	cvs_annotate(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_rannotate(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_rannotate: %s", strerror(errno));

	cvs_cmdop = CVS_OP_RANNOTATE;
	cmdp->cmd_flags = cvs_cmd_rannotate.cmd_flags;
	cvs_annotate(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_commit(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_commit: %s", strerror(errno));

	cvs_cmdop = CVS_OP_COMMIT;
	cmdp->cmd_flags = cvs_cmd_commit.cmd_flags;
	if (cvs_commit(server_argc, server_argv) != 0)
		cvs_server_send_response("error  commit failed");
	else
		cvs_server_send_response("ok");
}

void
cvs_server_checkout(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_checkout: %s", strerror(errno));

	cvs_cmdop = CVS_OP_CHECKOUT;
	cmdp->cmd_flags = cvs_cmd_checkout.cmd_flags;
	cvs_checkout(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_diff(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_diff: %s", strerror(errno));

	cvs_cmdop = CVS_OP_DIFF;
	cmdp->cmd_flags = cvs_cmd_diff.cmd_flags;
	cvs_diff(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_rdiff(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_rdiff: %s", strerror(errno));

	cvs_cmdop = CVS_OP_RDIFF;
	cmdp->cmd_flags = cvs_cmd_rdiff.cmd_flags;
	cvs_diff(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_export(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_export: %s", strerror(errno));

	cvs_cmdop = CVS_OP_EXPORT;
	cmdp->cmd_flags = cvs_cmd_export.cmd_flags;
	cvs_export(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_init(char *data)
{
	if (server_allowed_root != NULL)
		fatal("remote initialization is disabled for this server");
	if (data == NULL)
		fatal("Missing argument for init");

	if (current_cvsroot != NULL)
		fatal("Root in combination with init is not supported");

	if ((current_cvsroot = cvsroot_get(data)) == NULL)
		fatal("Invalid argument for init");

	cvs_cmdop = CVS_OP_INIT;
	cmdp->cmd_flags = cvs_cmd_init.cmd_flags;
	cvs_init(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_release(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_release: %s", strerror(errno));

	cvs_cmdop = CVS_OP_RELEASE;
	cmdp->cmd_flags = cvs_cmd_release.cmd_flags;
	cvs_release(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_remove(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_remove: %s", strerror(errno));

	cvs_cmdop = CVS_OP_REMOVE;
	cmdp->cmd_flags = cvs_cmd_remove.cmd_flags;
	cvs_remove(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_status(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_status: %s", strerror(errno));

	cvs_cmdop = CVS_OP_STATUS;
	cmdp->cmd_flags = cvs_cmd_status.cmd_flags;
	cvs_status(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_log(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_log: %s", strerror(errno));

	cvs_cmdop = CVS_OP_LOG;
	cmdp->cmd_flags = cvs_cmd_log.cmd_flags;
	cvs_getlog(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_rlog(char *data)
{
	(void)data;
	if (current_cvsroot == NULL)
		fatal("Root must precede rlog");
	if (chdir(current_cvsroot->cr_dir) == -1)
		fatal("cvs_server_rlog: %s", strerror(errno));

	cvs_cmdop = CVS_OP_RLOG;
	cmdp->cmd_flags = cvs_cmd_rlog.cmd_flags;
	cvs_getlog(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_tag(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_tag: %s", strerror(errno));

	cvs_cmdop = CVS_OP_TAG;
	cmdp->cmd_flags = cvs_cmd_tag.cmd_flags;
	cvs_tag(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_rtag(char *data)
{
	(void)data;
	if (current_cvsroot == NULL)
		fatal("Root must precede rtag");
	if (chdir(current_cvsroot->cr_dir) == -1)
		fatal("cvs_server_rtag: %s", strerror(errno));

	cvs_cmdop = CVS_OP_RTAG;
	cmdp->cmd_flags = cvs_cmd_rtag.cmd_flags;
	cvs_tag(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_update(char *data)
{
	(void)data;
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_update: %s", strerror(errno));

	cvs_cmdop = CVS_OP_UPDATE;
	cmdp->cmd_flags = cvs_cmd_update.cmd_flags;
	cvs_update(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_version(char *data)
{
	(void)data;
	cvs_cmdop = CVS_OP_VERSION;
	cmdp->cmd_flags = cvs_cmd_version.cmd_flags;
	cvs_version(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_update_entry(const char *resp, struct cvs_file *cf)
{
	char *p;
	char  repo[PATH_MAX], fpath[PATH_MAX];

	if ((p = strrchr(cf->file_rpath, ',')) != NULL)
		*p = '\0';

	cvs_get_repository_path(cf->file_wd, repo, PATH_MAX);
	(void)xsnprintf(fpath, PATH_MAX, "%s/%s", repo, cf->file_name);

	cvs_server_send_response("%s %s/", resp, cf->file_wd);
	cvs_remote_output(fpath);

	if (p != NULL)
		*p = ',';
}

void
cvs_server_set_sticky(const char *dir, char *tag)
{
	char fpath[PATH_MAX];
	char repo[PATH_MAX];

	cvs_get_repository_path(dir, repo, PATH_MAX);
	(void)xsnprintf(fpath, PATH_MAX, "%s/", repo);

	cvs_server_send_response("Set-sticky %s/", dir);
	cvs_remote_output(fpath);
	cvs_remote_output(tag);
}

void
cvs_server_clear_sticky(char *dir)
{
	char fpath[PATH_MAX];
	char repo[PATH_MAX];

	cvs_get_repository_path(dir, repo, PATH_MAX);
	(void)xsnprintf(fpath, PATH_MAX, "%s/", repo);

	cvs_server_send_response("Clear-sticky %s//", dir);
	cvs_remote_output(fpath);
}

void
cvs_server_exp_modules(char *module)
{
	(void)module;
	struct module_checkout *mo;
	struct cvs_filelist    *fl;

	if (server_argc != 2)
		fatal("expand-modules with no arguments");

	mo = cvs_module_lookup(server_argv[1]);

	RB_FOREACH(fl, cvs_flisthead, &(mo->mc_modules))
	cvs_server_send_response("Module-expansion %s", fl->file_path);
	if (mo->mc_canfree) {
		cvs_file_freelist(&mo->mc_modules);
		cvs_file_freelist(&mo->mc_ignores);
	}
	free(mo);
	cvs_server_send_response("ok");

	server_argc--;
	free(server_argv[1]);
	server_argv[1] = NULL;
}
