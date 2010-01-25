/*  Evolution RSS Reader Plugin
 *  Copyright (C) 2007-2010 Lucian Langa <cooly@gnome.eu.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef _FETCH_H_
#define _FETCH_H_ 1

#include <libsoup/soup.h>
#include "network.h"

GString *fetch_blocking(
	gchar *url,
	GSList *headers,
	GString *post,
	NetStatusCallback cb,
	gpointer data,
	GError **err);

gboolean fetch_unblocking(
	gchar *url,
	NetStatusCallback cb,
	gpointer data,
	gpointer cb2,
	gpointer cbdata2,
	guint track,
	GError **err);
#endif

