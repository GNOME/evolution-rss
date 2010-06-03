/*  Evoution RSS Reader Plugin
 *  Copyright (C) 2007-2009 Lucian Langa <cooly@gnome.eu.org>
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
 */

#if (EVOLUTION_VERSION >= 22900) //kb//
EActivity*
taskbar_op_message(gchar *msg, gchar *unikey);
#else
guint
taskbar_op_message(gchar *msg, gchar *unikey);
#endif
void taskbar_op_abort(gpointer key);
void taskbar_op_set_progress(gchar *key, gchar *msg, gdouble progress);
void taskbar_op_finish(gchar *key);
void taskbar_push_message(gchar *message);
void taskbar_pop_message(void);

