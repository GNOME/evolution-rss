/* Evoution RSS Reader Plugin
 * Copyright (C) 2011  Lucian Langa <lucilanga@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <mail/e-mail-local.h>
#include <mail/e-mail-reader.h>
#include <mail/em-folder-utils.h>

#include "rss-evo-common.h"

gboolean
rss_emfu_is_special_local_folder (const gchar *name)
{
	return (!strcmp (name, "Drafts") || !strcmp (name, "Inbox") || !strcmp (name, "Outbox") || !strcmp (name, "Sent") || !strcmp (name, "Templates"));
}

void
rss_emfu_copy_folder_selected (EMailBackend *backend,
			const gchar *uri,
			gpointer data)
{
	EMailSession *session;
	struct _copy_folder_data *cfd = data;
	CamelStore *fromstore = NULL, *tostore = NULL;
	CamelStore *local_store;
	const gchar *tobase = NULL;
	CamelURL *url;
	GError *local_error = NULL;

	if (uri == NULL) {
		g_free (cfd);
		return;
	}

	local_store = e_mail_local_get_store ();
	session = e_mail_backend_get_session (backend);

	fromstore = camel_session_get_store (
		CAMEL_SESSION (session), cfd->fi->uri, &local_error);
	if (fromstore == NULL) {
		e_mail_backend_submit_alert (
			backend, cfd->delete ?
				"mail:no-move-folder-notexist" :
				"mail:no-copy-folder-notexist",
			cfd->fi->full_name, uri,
			local_error->message, NULL);
		goto fail;
	}

	if (cfd->delete && fromstore == local_store && rss_emfu_is_special_local_folder (cfd->fi->full_name)) {
		e_mail_backend_submit_alert (
			backend, "mail:no-rename-special-folder",
			cfd->fi->full_name, NULL);
		goto fail;
	}

	tostore = camel_session_get_store (
		CAMEL_SESSION (session), uri, &local_error);
	if (tostore == NULL) {
		e_mail_backend_submit_alert (
			backend, cfd->delete ?
				"mail:no-move-folder-to-notexist" :
				"mail:no-copy-folder-to-notexist",
			cfd->fi->full_name, uri,
			local_error->message, NULL);
		goto fail;
	}

	url = camel_url_new (uri, NULL);
	if (((CamelService *)tostore)->provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
		tobase = url->fragment;
	else if (url->path && url->path[0])
		tobase = url->path+1;
	if (tobase == NULL)
		tobase = "";

	em_folder_utils_copy_folders (
		fromstore, cfd->fi->full_name, tostore, tobase, cfd->delete);

	camel_url_free (url);
fail:
	if (fromstore)
		g_object_unref (fromstore);
	if (tostore)
		g_object_unref (tostore);

	g_clear_error (&local_error);

	g_free (cfd);
}

