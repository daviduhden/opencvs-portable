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
/*	$OpenBSD: client.c,v 1.129 2020/10/19 19:51:20 naddy Exp $	*/
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
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cvs.h"
#include "pc_process.h"
#include <ctype.h>
#include "remote.h"

struct cvs_req cvs_requests[] = {
	/* this is what our client will use, the server should support it */
	{ "Root",		1,	cvs_server_root, REQ_NEEDED },
	{ "Valid-responses",	1,	cvs_server_validresp, REQ_NEEDED },
	{ "valid-requests",	1,	cvs_server_validreq, REQ_NEEDED },
	{ "Directory",		0,	cvs_server_directory, REQ_NEEDED },
	{ "Static-directory",	0,	cvs_server_static_directory,
	    REQ_NEEDED | REQ_NEEDDIR },
	{ "Sticky",		0,	cvs_server_sticky,
	    REQ_NEEDED | REQ_NEEDDIR },
	{ "Entry",		0,	cvs_server_entry,
	    REQ_NEEDED | REQ_NEEDDIR },
	{ "Modified",		0,	cvs_server_modified,
	    REQ_NEEDED | REQ_NEEDDIR },
	{ "UseUnchanged",	0,	cvs_server_useunchanged, REQ_NEEDED },
	{ "Unchanged",		0,	cvs_server_unchanged,
	    REQ_NEEDED | REQ_NEEDDIR },
	{ "Questionable",	0,	cvs_server_questionable, REQ_NEEDED },
	{ "Argument",		0,	cvs_server_argument, REQ_NEEDED },
	{ "Argumentx",		0,	cvs_server_argumentx, REQ_NEEDED },
	{ "Global_option",	0,	cvs_server_globalopt, REQ_NEEDED },
	{ "Set",		0,	cvs_server_set, REQ_NEEDED },
	{ "expand-modules",	0,	cvs_server_exp_modules, 0 },

	/*
	 * used to tell the server what is going on in our
	 * working copy, unsupported until we are told otherwise
	 */
	{ "Max-dotdot",			0,	NULL, 0 },
	{ "Checkin-prog",		0,	NULL, 0 },
	{ "Update-prog",		0,	NULL, 0 },
	{ "Kopt",			0,	NULL, 0 },
	{ "Checkin-time",		0,	NULL, 0 },
	{ "Is-modified",		0,	NULL, 0 },
	{ "Notify",			0,	NULL, 0 },
	{ "Case",			0,	NULL, 0 },
	{ "Gzip-stream",		0,	NULL, 0 },
	{ "wrapper-sendme-rcsOptions",	0,	NULL, 0 },
	{ "Kerberos-encrypt",		0,	NULL, 0 },
	{ "Gssapi-encrypt",		0,	NULL, 0 },
	{ "Gssapi-authenticate",	0,	NULL, 0 },

	/* commands that might be supported */
	{ "ci",			0,	cvs_server_commit,	REQ_NEEDDIR },
	{ "co",			0,	cvs_server_checkout,	REQ_NEEDDIR },
	{ "update",		0,	cvs_server_update,	REQ_NEEDDIR },
	{ "diff",		0,	cvs_server_diff,	REQ_NEEDDIR },
	{ "log",		0,	cvs_server_log,		REQ_NEEDDIR },
	{ "rlog",		0,	cvs_server_rlog, 0 },
	{ "add",		0,	cvs_server_add,		REQ_NEEDDIR },
	{ "remove",		0,	cvs_server_remove,	REQ_NEEDDIR },
	{ "update-patches",	0,	cvs_server_update_patches, 0 },
	{ "gzip-file-contents",	0,	NULL, 0 },
	{ "status",		0,	cvs_server_status,	REQ_NEEDDIR },
	{ "rdiff",		0,	cvs_server_rdiff, 0 },
	{ "tag",		0,	cvs_server_tag,		REQ_NEEDDIR },
	{ "rtag",		0,	cvs_server_rtag, 0 },
	{ "import",		0,	cvs_server_import,	REQ_NEEDDIR },
	{ "admin",		0,	cvs_server_admin,	REQ_NEEDDIR },
	{ "export",		0,	cvs_server_export,	REQ_NEEDDIR },
	{ "history",		0,	NULL, 0 },
	{ "release",		0,	cvs_server_release,	REQ_NEEDDIR },
	{ "watch-on",		0,	NULL, 0 },
	{ "watch-off",		0,	NULL, 0 },
	{ "watch-add",		0,	NULL, 0 },
	{ "watch-remove",	0,	NULL, 0 },
	{ "watchers",		0,	NULL, 0 },
	{ "editors",		0,	NULL, 0 },
	{ "init",		0,	cvs_server_init, 0 },
	{ "annotate",		0,	cvs_server_annotate,	REQ_NEEDDIR },
	{ "rannotate",		0,	cvs_server_rannotate, 0 },
	{ "noop",		0,	NULL, 0 },
	{ "version",		0,	cvs_server_version, 0 },
	{ "",			-1,	NULL, 0 }
};

static void	 client_check_directory(char *, char *);
static char	*client_get_supported_responses(void);
static char	*lastdir = NULL;
static int	 end_of_response = 0;

static void	cvs_client_initlog(void);

/*
 * File descriptors for protocol logging when the CVS_CLIENT_LOG environment
 * variable is set.
 */
static int	cvs_client_logon = 0;
int	cvs_client_inlog_fd = -1;
int	cvs_client_outlog_fd = -1;


int server_response = SERVER_OK;

static char *
client_get_supported_responses(void)
{
	BUF *bp;
	char *d;
	int i, first;

	first = 0;
	bp = buf_alloc(512);
	for (i = 0; cvs_responses[i].supported != -1; i++) {
		if (cvs_responses[i].hdlr == NULL)
			continue;

		if (first != 0)
			buf_putc(bp, ' ');
		else
			first++;
		buf_puts(bp, cvs_responses[i].name);
	}

	buf_putc(bp, '\0');
	d = buf_release(bp);
	return (d);
}

/* Server paths are protocol data, never native absolute working paths. */
static void
client_check_relative(const char *path)
{
	const char *component, *end;
	size_t length;

	if (path == NULL || *path == '\0' || pc_any_absolute(path) ||
	    strchr(path, '\\') != NULL || strchr(path, ':') != NULL)
		fatal("unsafe pathname received from server");
	for (component = path; *component != '\0'; component = end + 1) {
		end = strchr(component, '/');
		length = end ? (size_t)(end - component) : strlen(component);
		if (length == 0 || (length == 2 && !memcmp(component, "..", 2)))
			fatal("unsafe pathname received from server");
		if (end == NULL || end[1] == '\0')
			break;
	}
}

/* Reject already-present symlink prefixes before creating or removing files.
 * This is not a claim of race-free containment against concurrent directory swaps.
 */
static void
client_check_workdir(const char *path)
{
	char prefix[PATH_MAX], *end;
	struct stat st;
	int last;

	client_check_relative(path);
	if (strlcpy(prefix, path, sizeof(prefix)) >= sizeof(prefix))
		fatal("server directory path is too long");
	for (end = prefix;; end++) {
		if (*end != '/' && *end != '\0')
			continue;
		last = (*end == '\0');
		*end = '\0';
		if (lstat(prefix, &st) == -1) {
			if (errno != ENOENT)
				fatal("server directory %s: %s", prefix, strerror(errno));
		} else if (!S_ISDIR(st.st_mode)) {
			fatal("server directory is not a real directory: %s", prefix);
		}
		if (last)
			break;
		*end = '/';
	}
	(void)xsnprintf(prefix, sizeof(prefix), "%s/%s", path, CVS_PATH_CVSDIR);
	if (lstat(prefix, &st) == -1) {
		if (errno != ENOENT)
			fatal("server administrative directory: %s", strerror(errno));
	} else if (!S_ISDIR(st.st_mode)) {
		fatal("server administrative directory is not a real directory");
	}
}

static const char *
client_repository_relative(const char *path)
{
	size_t root_length, length;
	const char *relative;

	if (path == NULL || current_cvsroot == NULL ||
	    current_cvsroot->cr_dir == NULL)
		fatal("server response lacks a repository root");
	root_length = strlen(current_cvsroot->cr_dir);
	length = strlen(path);
	if (root_length == 0 || length < root_length ||
	    strncmp(path, current_cvsroot->cr_dir, root_length) != 0)
		fatal("server repository path is outside the selected root");
	relative = path + root_length;
	if (*relative == '\0')
		return relative;
	if (current_cvsroot->cr_dir[root_length - 1] != '/') {
		if (*relative != '/')
			fatal("server repository path has a different root");
		relative++;
	}
	client_check_relative(relative);
	return relative;
}

static const char *
client_repository_filename(const char *path)
{
	const char *relative = client_repository_relative(path);
	const char *name = strrchr(relative, '/');

	name = name ? name + 1 : relative;
	if (*name == '\0' || !strcmp(name, ".") || !strcmp(name, ".."))
		fatal("server response lacks a safe file name");
	return name;
}

/* Publish only after the complete body and metadata reached stable storage. */
static void
client_receive_file(const char *path, size_t length, mode_t mode, time_t now)
{
	char *temporary;
	struct timeval tv[2];
	int fd, saved;

	(void)xasprintf(&temporary, "%s.XXXXXXXX", path);
	fd = mkstemp(temporary);
	if (fd == -1)
		fatal("client temporary file: %s", strerror(errno));
	worklist_add(temporary, &temp_files);
	cvs_remote_receive_file(fd, length);
	tv[0].tv_sec = now;
	tv[0].tv_usec = 0;
	tv[1] = tv[0];
	if (futimes(fd, tv) == -1 || fchmod(fd, mode) == -1 || fsync(fd) == -1) {
		saved = errno;
		(void)close(fd);
		fatal("client staged file: %s", strerror(saved));
	}
	if (close(fd) == -1)
		fatal("client staged close: %s", strerror(errno));
	if (rename(temporary, path) == -1)
		fatal("client staged rename: %s", strerror(errno));
	worklist_remove(temporary, &temp_files);
	free(temporary);
}

static void
client_write_metadata(const char *path, const char *text)
{
	char *temporary;
	int fd, saved;
	FILE *stream;

	(void)xasprintf(&temporary, "%s.XXXXXXXX", path);
	fd = mkstemp(temporary);
	if (fd == -1)
		fatal("client administrative temporary file: %s", strerror(errno));
	worklist_add(temporary, &temp_files);
	stream = fdopen(fd, "w");
	if (stream == NULL) {
		saved = errno;
		(void)close(fd);
		fatal("client administrative stream: %s", strerror(saved));
	}
	if ((text != NULL && (fputs(text, stream) == EOF ||
	    fputc('\n', stream) == EOF)) || fflush(stream) == EOF ||
	    fsync(fd) == -1) {
		saved = errno;
		(void)fclose(stream);
		fatal("client administrative write: %s", strerror(saved));
	}
	if (fclose(stream) == EOF)
		fatal("client administrative close: %s", strerror(errno));
	if (rename(temporary, path) == -1)
		fatal("client administrative rename: %s", strerror(errno));
	worklist_remove(temporary, &temp_files);
	free(temporary);
}

static void
client_check_directory(char *data, char *repository)
{
	CVSENTRIES *entlist;
	char *entry, *parent, *base, *p;
	char basebuf[PATH_MAX], parentbuf[PATH_MAX];

	STRIP_SLASH(data);
	client_check_workdir(data);
	(void)client_repository_relative(repository);

	/* first directory we get is our module root */
	if (module_repo_root == NULL && checkout_target_dir != NULL) {
		p = xstrdup(client_repository_relative(repository));
		module_repo_root = p;
		p = strrchr(module_repo_root, '/');
		if (p != NULL)
			*p = '\0';
	}

	cvs_mkpath(data, NULL);

	if (cvs_cmdop == CVS_OP_EXPORT)
		return;

	if (strlcpy(basebuf, data, sizeof(basebuf)) >= sizeof(basebuf))
		fatal("client_check_directory: truncation");
	if ((base = basename(basebuf)) == NULL)
		fatal("client_check_directory: overflow");

	if (strlcpy(parentbuf, data, sizeof(parentbuf)) >= sizeof(parentbuf))
		fatal("client_check_directory: truncation");
	if ((parent = dirname(parentbuf)) == NULL)
		fatal("client_check_directory: overflow");

	if (!strcmp(parent, "."))
		return;

	entry = xmalloc(CVS_ENT_MAXLINELEN);
	cvs_ent_line_str(base, NULL, NULL, NULL, NULL, 1, 0, entry,
	    CVS_ENT_MAXLINELEN);

	entlist = cvs_ent_open(parent);
	cvs_ent_add(entlist, entry);

	free(entry);
}

static pc_process cvs_client_process;

void
cvs_client_disconnect(void)
{
	int status;
	if (cvs_client_process.handle == 0)
		return;
	if (current_cvsroot->cr_srvin != NULL) {
		(void)fclose(current_cvsroot->cr_srvin);
		current_cvsroot->cr_srvin = NULL;
	}
	if (current_cvsroot->cr_srvout != NULL) {
		(void)fclose(current_cvsroot->cr_srvout);
		current_cvsroot->cr_srvout = NULL;
	}
	if (pc_wait(&cvs_client_process, &status) == -1)
		cvs_log(LP_ERR, "unable to wait for SSH transport: %s", strerror(errno));
}

void
cvs_client_connect_to_server(void)
{
	struct cvs_var *vp;
	char *cmd, *argv[64], *resp, **rsh;
	char port[6];
	int ifd[2], ofd[2], argc, index;

	if (cvs_server_active == 1)
		fatal("cvs_client_connect: I was already connected to server");

	switch (current_cvsroot->cr_method) {
	case CVS_METHOD_PSERVER:
	case CVS_METHOD_KSERVER:
	case CVS_METHOD_GSERVER:
	case CVS_METHOD_FORK:
		fatal("the specified connection method is not supported");
	default:
		break;
	}

	if (pc_pipe(ifd) == -1)
		fatal("cvs_client_connect: %s", strerror(errno));
	if (pc_pipe(ofd) == -1)
		fatal("cvs_client_connect: %s", strerror(errno));

	if ((cmd = getenv("CVS_SERVER")) == NULL)
		cmd = CVS_SERVER_DEFAULT;
	/* SSH joins remote command arguments through the remote login shell. */
	if (*cmd == '\0' || strspn(cmd,
	    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_./:-")
	    != strlen(cmd))
		fatal("CVS_SERVER must be a bare executable or a safe absolute path");
	rsh = pc_command_argv(cvs_rsh, NULL);
	if (rsh == NULL)
		fatal("invalid CVS_RSH command");
	argc = 0;
	for (index = 0; rsh[index] != NULL; index++) {
		if (argc >= 54)
			fatal("too many CVS_RSH arguments");
		argv[argc++] = rsh[index];
	}
	if (current_cvsroot->cr_user != NULL) {
		argv[argc++] = "-l";
		argv[argc++] = current_cvsroot->cr_user;
	}
	if (current_cvsroot->cr_port != 0) {
		(void)xsnprintf(port, sizeof(port), "%u", current_cvsroot->cr_port);
		argv[argc++] = "-p";
		argv[argc++] = port;
	}
	argv[argc++] = "--";
	argv[argc++] = current_cvsroot->cr_host;
	argv[argc++] = cmd;
	argv[argc++] = "server";
	argv[argc] = NULL;
	if (pc_spawn(&cvs_client_process, (const char *const *)argv,
	    ifd[0], ofd[1], -1) == -1)
		fatal("cvs_client_connect: spawn: %s", strerror(errno));
	pc_free_argv(rsh);

	close(ifd[0]);
	close(ofd[1]);

	if ((current_cvsroot->cr_srvin = fdopen(ifd[1], "w")) == NULL)
		fatal("cvs_client_connect: %s", strerror(errno));
	if ((current_cvsroot->cr_srvout = fdopen(ofd[0], "r")) == NULL)
		fatal("cvs_client_connect: %s", strerror(errno));

	if (setvbuf(current_cvsroot->cr_srvin, NULL, _IONBF, 0) != 0 ||
	    setvbuf(current_cvsroot->cr_srvout, NULL, _IONBF, 0) != 0)
		fatal("cvs_client_connect: cannot configure protocol streams");

	cvs_client_initlog();

	if (cvs_cmdop != CVS_OP_INIT)
		cvs_client_send_request("Root %s", current_cvsroot->cr_dir);

	resp = client_get_supported_responses();
	cvs_client_send_request("Valid-responses %s", resp);
	free(resp);

	cvs_client_send_request("valid-requests");
	cvs_client_get_responses();

	cvs_client_send_request("UseUnchanged");

	if (cvs_nolog == 1)
		cvs_client_send_request("Global_option -l");

	if (cvs_noexec == 1)
		cvs_client_send_request("Global_option -n");

	switch (verbosity) {
	case 0:
		cvs_client_send_request("Global_option -Q");
		break;
	case 1:
		/* Be quiet. This is the default in OpenCVS. */
		cvs_client_send_request("Global_option -q");
		break;
	default:
		break;
	}

	if (cvs_readonly == 1)
		cvs_client_send_request("Global_option -r");

	if (cvs_trace == 1)
		cvs_client_send_request("Global_option -t");

	/* XXX: If 'Set' is supported? */
	TAILQ_FOREACH(vp, &cvs_variables, cv_link)
		cvs_client_send_request("Set %s=%s", vp->cv_name, vp->cv_val);
}

void
cvs_client_send_request(char *fmt, ...)
{
	int i;
	va_list ap;
	char *data, *s;
	struct cvs_req *req;

	va_start(ap, fmt);
	i = vasprintf(&data, fmt, ap);
	va_end(ap);
	if (i == -1)
		fatal("cvs_client_send_request: could not allocate memory");

	if ((s = strchr(data, ' ')) != NULL)
		*s = '\0';

	req = cvs_remote_get_request_info(data);
	if (req == NULL)
		fatal("'%s' is an unknown request", data);

	if (req->supported != 1)
		fatal("remote cvs server does not support '%s'", data);

	if (s != NULL)
		*s = ' ';

	cvs_log(LP_TRACE, "%s", data);

	cvs_remote_output(data);
	free(data);
}

void
cvs_client_read_response(void)
{
	char *cmd, *data;
	struct cvs_resp *resp;

	cmd = cvs_remote_input();
	if ((data = strchr(cmd, ' ')) != NULL)
		(*data++) = '\0';

	resp = cvs_remote_get_response_info(cmd);
	if (resp == NULL)
		fatal("response '%s' is not supported by our client", cmd);

	if (resp->hdlr == NULL)
		fatal("opencvs client does not support '%s'", cmd);

	(*resp->hdlr)(data);

	free(cmd);
}

void
cvs_client_get_responses(void)
{
	while (end_of_response != 1)
		cvs_client_read_response();

	end_of_response = 0;
}

void
cvs_client_send_logmsg(char *msg)
{
	char *buf, *p, *q;

	(void)xasprintf(&buf, "%s\n", msg);

	cvs_client_send_request("Argument -m");
	if ((p = strchr(buf, '\n')) != NULL)
		*p++ = '\0';
	cvs_client_send_request("Argument %s", buf);
	for (q = p; p != NULL; q = p) {
		if ((p = strchr(q, '\n')) != NULL) {
			*p++ = '\0';
			cvs_client_send_request("Argumentx %s", q);
		}
	}

	free(buf);
}

void
cvs_client_senddir(const char *dir)
{
	struct stat st;
	int nb;
	char *d, *date, fpath[PATH_MAX], repo[PATH_MAX], *tag;

	d = NULL;

	if (lastdir != NULL && !strcmp(dir, lastdir))
		return;

	cvs_get_repository_path(dir, repo, PATH_MAX);

	if (cvs_cmdop != CVS_OP_RLOG)
		cvs_client_send_request("Directory %s\n%s", dir, repo);

	(void)xsnprintf(fpath, PATH_MAX, "%s/%s",
	    dir, CVS_PATH_STATICENTRIES);

	if (stat(fpath, &st) == 0 && (st.st_mode & (S_IRUSR|S_IRGRP|S_IROTH)))
		cvs_client_send_request("Static-directory");

	d = xstrdup(dir);
	cvs_parse_tagfile(d, &tag, &date, &nb);

	if (tag != NULL || date != NULL) {
		char buf[128];

		if (tag != NULL && nb != 0) {
			if (strlcpy(buf, "N", sizeof(buf)) >= sizeof(buf))
				fatal("cvs_client_senddir: truncation");
		} else if (tag != NULL) {
			if (strlcpy(buf, "T", sizeof(buf)) >= sizeof(buf))
				fatal("cvs_client_senddir: truncation");
		} else {
			if (strlcpy(buf, "D", sizeof(buf)) >= sizeof(buf))
				fatal("cvs_client_senddir: truncation");
		}

		if (strlcat(buf, tag ? tag : date, sizeof(buf)) >= sizeof(buf))
			fatal("cvs_client_senddir: truncation");

		cvs_client_send_request("Sticky %s", buf);

		free(tag);
		free(date);
	}
	free(d);
	free(lastdir);
	lastdir = xstrdup(dir);
}

void
cvs_client_sendfile(struct cvs_file *cf)
{
	size_t len;
	struct tm datetm;
	char rev[CVS_REV_BUFSZ], timebuf[CVS_TIME_BUFSZ], sticky[CVS_REV_BUFSZ];

	cvs_client_senddir(cf->file_wd);
	cvs_remote_classify_file(cf);

	if (cf->file_type == CVS_DIR)
		return;

	if (cf->file_ent != NULL && cvs_cmdop != CVS_OP_IMPORT) {
		if (cf->file_status == FILE_ADDED) {
			len = strlcpy(rev, "0", sizeof(rev));
			if (len >= sizeof(rev))
				fatal("cvs_client_sendfile: truncation");

			len = strlcpy(timebuf, "Initial ", sizeof(timebuf));
			if (len >= sizeof(timebuf))
				fatal("cvs_client_sendfile: truncation");

			len = strlcat(timebuf, cf->file_name, sizeof(timebuf));
			if (len >= sizeof(timebuf))
				fatal("cvs_client_sendfile: truncation");
		} else {
			rcsnum_tostr(cf->file_ent->ce_rev, rev, sizeof(rev));
			ctime_r(&cf->file_ent->ce_mtime, timebuf);
		}

		if (cf->file_ent->ce_conflict == NULL) {
			timebuf[strcspn(timebuf, "\n")] = '\0';
		} else {
			len = strlcpy(timebuf, cf->file_ent->ce_conflict,
			    sizeof(timebuf));
			if (len >= sizeof(timebuf))
				fatal("cvs_client_sendfile: truncation");
			len = strlcat(timebuf, "+=", sizeof(timebuf));
			if (len >= sizeof(timebuf))
				fatal("cvs_client_sendfile: truncation");
		}

		sticky[0] = '\0';
		if (cf->file_ent->ce_tag != NULL) {
			(void)xsnprintf(sticky, sizeof(sticky), "T%s",
			    cf->file_ent->ce_tag);
		} else if (cf->file_ent->ce_date != -1) {
			gmtime_r(&(cf->file_ent->ce_date), &datetm);
			(void)strftime(sticky, sizeof(sticky),
			    "D"CVS_DATE_FMT, &datetm);
		}

		cvs_client_send_request("Entry /%s/%s%s/%s/%s/%s",
		    cf->file_name, (cf->file_status == FILE_REMOVED) ? "-" : "",
		    rev, timebuf, cf->file_ent->ce_opts ?
		    cf->file_ent->ce_opts : "", sticky);
	}

	if (cvs_cmdop == CVS_OP_ADD)
		cf->file_status = FILE_MODIFIED;

	switch (cf->file_status) {
	case CVS_FILE_UNKNOWN:
		if (cf->file_flags & FILE_ON_DISK)
			cvs_client_send_request("Questionable %s",
			    cf->file_name);
		break;
	case FILE_ADDED:
		if (backup_local_changes)	/* for update -C */
			cvs_backup_file(cf);

		cvs_client_send_request("Modified %s", cf->file_name);
		cvs_remote_send_file(cf->file_path, cf->fd);
		break;
	case FILE_MODIFIED:
		if (backup_local_changes) {	/* for update -C */
			cvs_backup_file(cf);
			cvs_client_send_request("Entry /%s/%s%s/%s/%s/%s",
			    cf->file_name, "", rev, timebuf,
			    cf->file_ent->ce_opts ? cf->file_ent->ce_opts : "",
			    sticky);
			break;
		}

		cvs_client_send_request("Modified %s", cf->file_name);
		cvs_remote_send_file(cf->file_path, cf->fd);
		break;
	case FILE_UPTODATE:
		cvs_client_send_request("Unchanged %s", cf->file_name);
		break;
	}
}

void
cvs_client_send_files(char **argv, int argc)
{
	int i;

	for (i = 0; i < argc; i++)
		cvs_client_send_request("Argument %s", argv[i]);
}

void
cvs_client_ok(char *data)
{
	(void)data;
	end_of_response = 1;
	server_response = SERVER_OK;
}

void
cvs_client_error(char *data)
{
	(void)data;
	end_of_response = 1;
	server_response = SERVER_ERROR;
}

void
cvs_client_validreq(char *data)
{
	int i;
	char *sp, *ep;
	struct cvs_req *req;

	if ((sp = data) == NULL)
		fatal("Missing argument for Valid-requests");

	do {
		if ((ep = strchr(sp, ' ')) != NULL)
			*ep = '\0';

		req = cvs_remote_get_request_info(sp);
		if (req != NULL)
			req->supported = 1;

		if (ep != NULL)
			sp = ep + 1;
	} while (ep != NULL);

	for (i = 0; cvs_requests[i].supported != -1; i++) {
		req = &cvs_requests[i];
		if ((req->flags & REQ_NEEDED) &&
		    req->supported != 1) {
			fatal("server does not support required '%s'",
			    req->name);
		}
	}
}

void
cvs_client_e(char *data)
{
	if (data == NULL)
		fatal("Missing argument for E");

	fprintf(stderr, "%s\n", data);
}

void
cvs_client_m(char *data)
{
	if (data == NULL)
		fatal("Missing argument for M");

	puts(data);
}

void
cvs_client_checkedin(char *data)
{
	CVSENTRIES *entlist;
	struct cvs_ent *ent, *newent;
	size_t len;
	struct tm datetm;
	char *dir, *e, *entry, rev[CVS_REV_BUFSZ];
	char sticky[CVS_ENT_MAXLINELEN], timebuf[CVS_TIME_BUFSZ];

	if (data == NULL)
		fatal("Missing argument for Checked-in");

	dir = cvs_remote_input();
	e = cvs_remote_input();
	client_check_workdir(data);
	newent = cvs_ent_parse(e);
	if (strcmp(newent->ce_name, client_repository_filename(dir)))
		fatal("Checked-in file and Entries names disagree");
	free(dir);

	entlist = cvs_ent_open(data);
	ent = cvs_ent_get(entlist, newent->ce_name);
	free(e);

	rcsnum_tostr(newent->ce_rev, rev, sizeof(rev));

	sticky[0] = '\0';
	if (ent == NULL) {
		len = strlcpy(rev, "0", sizeof(rev));
		if (len >= sizeof(rev))
			fatal("cvs_client_sendfile: truncation");

		len = strlcpy(timebuf, "Initial ", sizeof(timebuf));
		if (len >= sizeof(timebuf))
			fatal("cvs_client_sendfile: truncation");

		len = strlcat(timebuf, newent->ce_name, sizeof(timebuf));
		if (len >= sizeof(timebuf))
			fatal("cvs_client_sendfile: truncation");
	} else {
		gmtime_r(&ent->ce_mtime, &datetm);
		asctime_r(&datetm, timebuf);
		timebuf[strcspn(timebuf, "\n")] = '\0';

		if (newent->ce_tag != NULL) {
			(void)xsnprintf(sticky, sizeof(sticky), "T%s",
			    newent->ce_tag);
		} else if (newent->ce_date != -1) {
			gmtime_r(&(newent->ce_date), &datetm);
			(void)strftime(sticky, sizeof(sticky),
			    "D"CVS_DATE_FMT, &datetm);
		}

		cvs_ent_free(ent);
	}

	entry = xmalloc(CVS_ENT_MAXLINELEN);
	cvs_ent_line_str(newent->ce_name, rev, timebuf,
	    newent->ce_opts ? newent->ce_opts : "", sticky, 0,
	    newent->ce_status == CVS_ENT_REMOVED ? 1 : 0,
	    entry, CVS_ENT_MAXLINELEN);

	cvs_ent_free(newent);
	cvs_ent_add(entlist, entry);

	free(entry);
}

void
cvs_client_updated(char *data)
{
	time_t now;
	mode_t fmode;
	size_t flen;
	CVSENTRIES *ent;
	struct cvs_ent *e;
	const char *errstr, *p;
	struct tm datetm;
	char *entry;
	char timebuf[CVS_TIME_BUFSZ], revbuf[CVS_REV_BUFSZ];
	char *en, *mode, *len, *rpath;
	char sticky[CVS_ENT_MAXLINELEN], fpath[PATH_MAX];

	if (data == NULL)
		fatal("Missing argument for Updated");

	rpath = cvs_remote_input();
	en = cvs_remote_input();
	mode = cvs_remote_input();
	len = cvs_remote_input();

	client_check_directory(data, rpath);
	p = client_repository_filename(rpath);
	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s", data, p);

	flen = strtonum(len, 0, INT_MAX, &errstr);
	if (errstr != NULL)
		fatal("cvs_client_updated: %s: %s", len, errstr);
	free(len);

	cvs_strtomode(mode, &fmode);
	free(mode);
	fmode &= ~cvs_umask;

	time(&now);
	gmtime_r(&now, &datetm);
	asctime_r(&datetm, timebuf);
	timebuf[strcspn(timebuf, "\n")] = '\0';

	e = cvs_ent_parse(en);
	if (strcmp(e->ce_name, p))
		fatal("Updated file and Entries names disagree");
	free(en);

	sticky[0] = '\0';
	if (e->ce_tag != NULL) {
		(void)xsnprintf(sticky, sizeof(sticky), "T%s", e->ce_tag);
	} else if (e->ce_date != -1) {
		gmtime_r(&(e->ce_date), &datetm);
		(void)strftime(sticky, sizeof(sticky),
		    "D"CVS_DATE_FMT, &datetm);
	}

	rcsnum_tostr(e->ce_rev, revbuf, sizeof(revbuf));

	entry = xmalloc(CVS_ENT_MAXLINELEN);
	cvs_ent_line_str(e->ce_name, revbuf, timebuf,
	    e->ce_opts ? e->ce_opts : "", sticky, 0, 0,
	    entry, CVS_ENT_MAXLINELEN);

	cvs_ent_free(e);

	client_receive_file(fpath, flen, fmode, now);
	if (cvs_cmdop != CVS_OP_EXPORT) {
		ent = cvs_ent_open(data);
		cvs_ent_add(ent, entry);
	}
	free(entry);

	free(rpath);
}

void
cvs_client_merged(char *data)
{
	time_t now;
	mode_t fmode;
	size_t flen;
	CVSENTRIES *ent;
	struct cvs_ent *parsed;
	const char *errstr, *name;
	char *rpath, *entry, *mode, *len, fpath[PATH_MAX];

	if (data == NULL)
		fatal("Missing argument for Merged");

	rpath = cvs_remote_input();
	entry = cvs_remote_input();
	mode = cvs_remote_input();
	len = cvs_remote_input();

	client_check_directory(data, rpath);

	name = client_repository_filename(rpath);
	(void)xsnprintf(fpath, sizeof(fpath), "%s/%s", data, name);
	parsed = cvs_ent_parse(entry);
	if (strcmp(parsed->ce_name, name))
		fatal("Merged file and Entries names disagree");
	cvs_ent_free(parsed);

	flen = strtonum(len, 0, INT_MAX, &errstr);
	if (errstr != NULL)
		fatal("cvs_client_merged: %s: %s", len, errstr);
	free(len);

	cvs_strtomode(mode, &fmode);
	free(mode);
	fmode &= ~cvs_umask;

	time(&now);
	client_receive_file(fpath, flen, fmode, now);
	ent = cvs_ent_open(data);
	cvs_ent_add(ent, entry);
	free(entry);

	free(rpath);
}

void
cvs_client_removed(char *data)
{
	CVSENTRIES *entlist;
	char *rpath, fpath[PATH_MAX];
	const char *filename;

	if (data == NULL)
		fatal("Missing argument for Removed");

	rpath = cvs_remote_input();
	client_check_workdir(data);
	filename = client_repository_filename(rpath);

	entlist = cvs_ent_open(data);
	(void)xsnprintf(fpath, PATH_MAX, "%s/%s", data, filename);
	if (unlink(fpath) == -1 && errno != ENOENT)
		fatal("Removed: %s: %s", fpath, strerror(errno));
	cvs_ent_remove(entlist, filename);

	free(rpath);
}

void
cvs_client_remove_entry(char *data)
{
	CVSENTRIES *entlist;
	char *rpath;
	const char *filename;

	if (data == NULL)
		fatal("Missing argument for Remove-entry");

	rpath = cvs_remote_input();
	client_check_workdir(data);
	filename = client_repository_filename(rpath);

	entlist = cvs_ent_open(data);
	cvs_ent_remove(entlist, filename);

	free(rpath);
}

void
cvs_client_set_static_directory(char *data)
{
	char *dir, fpath[PATH_MAX];

	if (data == NULL)
		fatal("Missing argument for Set-static-directory");

	STRIP_SLASH(data);

	dir = cvs_remote_input();
	client_check_workdir(data);
	(void)client_repository_relative(dir);
	free(dir);

	if (cvs_cmdop == CVS_OP_EXPORT)
		return;

	(void)xsnprintf(fpath, PATH_MAX, "%s/%s",
	    data, CVS_PATH_STATICENTRIES);

	client_write_metadata(fpath, NULL);
}

void
cvs_client_clear_static_directory(char *data)
{
	char *dir, fpath[PATH_MAX];

	if (data == NULL)
		fatal("Missing argument for Clear-static-directory");

	STRIP_SLASH(data);

	dir = cvs_remote_input();
	client_check_workdir(data);
	(void)client_repository_relative(dir);
	free(dir);

	if (cvs_cmdop == CVS_OP_EXPORT)
		return;

	(void)xsnprintf(fpath, PATH_MAX, "%s/%s",
	    data, CVS_PATH_STATICENTRIES);

	(void)cvs_unlink(fpath);
}

void
cvs_client_set_sticky(char *data)
{
	char *dir, *tag, tagpath[PATH_MAX];

	if (data == NULL)
		fatal("Missing argument for Set-sticky");

	STRIP_SLASH(data);

	dir = cvs_remote_input();
	tag = cvs_remote_input();

	if (cvs_cmdop == CVS_OP_EXPORT)
		goto out;

	client_check_directory(data, dir);

	(void)xsnprintf(tagpath, PATH_MAX, "%s/%s", data, CVS_PATH_TAG);

	client_write_metadata(tagpath, tag);
out:
	free(tag);
	free(dir);
}

void
cvs_client_clear_sticky(char *data)
{
	char *dir, tagpath[PATH_MAX];

	if (data == NULL)
		fatal("Missing argument for Clear-sticky");

	STRIP_SLASH(data);

	dir = cvs_remote_input();

	if (cvs_cmdop == CVS_OP_EXPORT) {
		free(dir);
		return;
	}

	client_check_directory(data, dir);

	(void)xsnprintf(tagpath, PATH_MAX, "%s/%s", data, CVS_PATH_TAG);
	(void)unlink(tagpath);

	free(dir);
}


/*
 * cvs_client_initlog()
 *
 * Initialize protocol logging if the CVS_CLIENT_LOG environment variable is
 * set.  In this case, the variable's value is used as a path to which the
 * appropriate suffix is added (".in" for client input and ".out" for server
 * output).
 */
static void
cvs_client_initlog(void)
{
	u_int i;
	char *env, *envdup, buf[PATH_MAX], fpath[PATH_MAX];
	char rpath[PATH_MAX], timebuf[CVS_TIME_BUFSZ], *s;
	time_t now;
	struct passwd *pwd;

	/* avoid doing it more than once */
	if (cvs_client_logon)
		return;

	if ((env = getenv("CVS_CLIENT_LOG")) == NULL)
		return;

	envdup = xstrdup(env);
	if ((s = strchr(envdup, '%')) != NULL)
		*s = '\0';

	if (strlcpy(buf, env, sizeof(buf)) >= sizeof(buf))
		fatal("cvs_client_initlog: truncation");

	if (strlcpy(rpath, envdup, sizeof(rpath)) >= sizeof(rpath))
		fatal("cvs_client_initlog: truncation");

	free(envdup);

	s = buf;
	while ((s = strchr(s, '%')) != NULL) {
		s++;
		switch (*s) {
		case 'c':
			if (strlcpy(fpath, cmdp->cmd_name, sizeof(fpath)) >=
			    sizeof(fpath))
				fatal("cvs_client_initlog: truncation");
			break;
		case 'd':
			time(&now);
			ctime_r(&now, timebuf);
			timebuf[strcspn(timebuf, "\n")] = '\0';
			if (strlcpy(fpath, timebuf, sizeof(fpath)) >=
			    sizeof(fpath))
				fatal("cvs_client_initlog: truncation");
			break;
		case 'p':
			(void)xsnprintf(fpath, sizeof(fpath), "%d", getpid());
			break;
		case 'u':
			if ((pwd = getpwuid(getuid())) != NULL) {
				if (strlcpy(fpath, pwd->pw_name,
				    sizeof(fpath)) >= sizeof(fpath))
					fatal("cvs_client_initlog: truncation");
			} else {
				fpath[0] = '\0';
			}
			endpwent();
			break;
		default:
			fpath[0] = '\0';
			break;
		}

		if (fpath[0] != '\0') {
			if (strlcat(rpath, "-", sizeof(rpath)) >= sizeof(rpath))
				fatal("cvs_client_initlog: truncation");

			if (strlcat(rpath, fpath, sizeof(rpath))
			    >= sizeof(rpath))
				fatal("cvs_client_initlog: truncation");
		}
	}

	for (i = 0;; i++) {
		(void)xsnprintf(fpath, sizeof(fpath), "%s-%u.in", rpath, i);
		cvs_client_inlog_fd = open(fpath,
		    O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
		if (cvs_client_inlog_fd != -1)
			break;
		if (errno != EEXIST || i == UINT_MAX)
			fatal("cvs_client_initlog: %s: %s", fpath, strerror(errno));
	}
	for (i = 0;; i++) {
		(void)xsnprintf(fpath, sizeof(fpath), "%s-%u.out", rpath, i);
		cvs_client_outlog_fd = open(fpath,
		    O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
		if (cvs_client_outlog_fd != -1)
			break;
		if (errno != EEXIST || i == UINT_MAX)
			fatal("cvs_client_initlog: %s: %s", fpath, strerror(errno));
	}

	cvs_client_logon = 1;
}
