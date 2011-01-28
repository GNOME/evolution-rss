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

#ifndef __RSS_EVO_COMMON_H_
#define __RSS_EVO_COMMON_H_

struct _copy_folder_data {
	CamelFolderInfo *fi;
	gboolean delete;
};

gboolean
rss_emfu_is_special_local_folder (const gchar *name);

void
rss_emfu_copy_folder_selected (EMailBackend *backend,
		const gchar *uri,
		gpointer data);

#endif /*__RSS_EVO_COMMON_H_*/

