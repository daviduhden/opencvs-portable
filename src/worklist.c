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
/*	$OpenBSD: worklist.c,v 1.8 2015/11/05 09:48:21 nicm Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"

/*
 * adds a path to a worklist.
 */
void
worklist_add(const char *path, struct wklhead *worklist)
{
	size_t len;
	struct worklist *wkl;

	wkl = xcalloc(1, sizeof(*wkl));

	len = strlcpy(wkl->wkl_path, path, sizeof(wkl->wkl_path));
	if (len >= sizeof(wkl->wkl_path))
		fatal("path truncation in worklist_add");

	SLIST_INSERT_HEAD(worklist, wkl, wkl_list);
}

/*
 * run over the given worklist, calling cb for each element.
 * Signal handlers only set atomic flags; list mutations and cleanup run
 * on the main thread. No POSIX signal-mask emulation is needed.
 */
void
worklist_run(struct wklhead *list, void (*cb)(struct worklist *))
{
	struct worklist *wkl;


	worklist_clean(list, cb);

	while ((wkl = SLIST_FIRST(list)) != NULL) {
		SLIST_REMOVE_HEAD(list, wkl_list);
		free(wkl);
	}

}

/*
 * pass elements to the specified callback, which has to be signal safe.
 */
void
worklist_clean(struct wklhead *list, void (*cb)(struct worklist *))
{
	struct worklist *wkl;

	SLIST_FOREACH(wkl, list, wkl_list)
	    cb(wkl);
}

void
worklist_unlink(struct worklist *wkl)
{
	(void)unlink(wkl->wkl_path);
}

void
worklist_remove(const char *path, struct wklhead *list)
{
	struct worklist *entry, *previous = NULL;
	SLIST_FOREACH(entry, list, wkl_list) {
		if (!strcmp(entry->wkl_path, path)) {
			if (previous == NULL)
				SLIST_REMOVE_HEAD(list, wkl_list);
			else
				SLIST_NEXT(previous, wkl_list) = SLIST_NEXT(entry, wkl_list);
			free(entry);
			return;
		}
		previous = entry;
	}
}
