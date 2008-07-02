/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Marco Pesenti Gritti
 * Copyright (C) 2008 Lucian Langa
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "mozilla-config.h"
#include "config.h"

#include <stdlib.h>

#include <nsStringAPI.h>

#ifdef HAVE_GECKO_1_9
#include <gtkmozembed.h>
#include <gtkmozembed_internal.h>
#else
#include <gtkembedmoz/gtkmozembed.h>
#include <gtkembedmoz/gtkmozembed_internal.h>
#endif
//#include <gtkmozembed.h>
#include <nsCOMPtr.h>
#include <nsIPrefService.h>
#include <nsIServiceManager.h>
#include <nsServiceManagerUtils.h>

static nsIPrefBranch* gPrefBranch;

extern "C" gboolean
gecko_prefs_set_bool (const gchar *key, gboolean value)
{
	NS_ENSURE_TRUE (gPrefBranch, FALSE);

	return NS_SUCCEEDED(gPrefBranch->SetBoolPref (key, value));
}

extern "C" gboolean
gecko_prefs_set_string (const gchar *key, const gchar *value)
{
	NS_ENSURE_TRUE (gPrefBranch, FALSE);

	return NS_SUCCEEDED(gPrefBranch->SetCharPref (key, value));
}

static gboolean
gecko_prefs_set_int (const gchar *key, gint value)
{
	NS_ENSURE_TRUE (gPrefBranch, FALSE);

	return NS_SUCCEEDED(gPrefBranch->SetIntPref (key, value));
}

extern "C" gboolean
gecko_init (void)
{
#ifdef HAVE_GECKO_1_9
	NS_LogInit ();
#endif
	
#ifdef HAVE_GECKO_1_9
	gtk_moz_embed_set_path (GECKO_HOME);
#else
	gtk_moz_embed_set_comp_path (GECKO_HOME);
#endif

	gtk_moz_embed_push_startup ();

	nsresult rv;
	nsCOMPtr<nsIPrefService> prefService (do_GetService (NS_PREFSERVICE_CONTRACTID, &rv));
	NS_ENSURE_SUCCESS (rv, FALSE);

	rv = CallQueryInterface (prefService, &gPrefBranch);
	NS_ENSURE_SUCCESS (rv, FALSE);
	
	return TRUE;
}

extern "C" void
gecko_shutdown (void)
{
	NS_IF_RELEASE (gPrefBranch);
	gPrefBranch = nsnull;

	gtk_moz_embed_pop_startup ();

#ifdef HAVE_GECKO_1_9
        NS_LogTerm ();
#endif
}
