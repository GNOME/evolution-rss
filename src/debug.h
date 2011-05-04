/* Evoution RSS Reader Plugin
 * Copyright (C) 2007-2009  Lucian Langa <cooly@gnome.eu.org>
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

#ifndef __DEBUG_H__
#define __DEBUG_H__ 1

#define d(f, x...) if (rss_verbose_debug) { g_print("%s: In function ‘%s‘:\n%s:%d:  ", __FILE__, \
			__FUNCTION__, __FILE__, __LINE__);\
			g_print(f, ## x); \
			g_print("\n");}

#define dp(f, x...) { g_print("%s(%d) %s():", __FILE__, __LINE__, __FUNCTION__);\
			g_print(f, ## x);}

#endif /*__DEBUG_H__*/

