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
 
#ifdef XPCOM_GLUE
#include <nsXPCOMGlue.h>
#include <gtkmozembed_glue.cpp>
#endif

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
#include <nspr.h>

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
       nsresult rv;
#ifdef HAVE_GECKO_1_9
	NS_LogInit ();
#endif

#ifdef XPCOM_GLUE
       static const GREVersionRange greVersion = {
         "1.9a", PR_TRUE,
         "2", PR_TRUE
       };
       char xpcomLocation[4096];
       rv = GRE_GetGREPathWithProperties(&greVersion, 1, nsnull, 0, xpcomLocation, 4096);
       if (NS_FAILED (rv))
       {
         g_warning ("Could not determine locale!\n");
         return FALSE;
       }

       // Startup the XPCOM Glue that links us up with XPCOM.
       rv = XPCOMGlueStartup(xpcomLocation);
       if (NS_FAILED (rv))
       {
         g_warning ("Could not determine locale!\n");
         return FALSE;
       }

       rv = GTKEmbedGlueStartup();
       if (NS_FAILED (rv))
       {
         g_warning ("Could not startup glue!\n");
         return FALSE;
       }

       rv = GTKEmbedGlueStartupInternal();
       if (NS_FAILED (rv))
       {
         g_warning ("Could not startup internal glue!\n");
         return FALSE;
       }

       char *lastSlash = strrchr(xpcomLocation, '/');
       if (lastSlash)
         *lastSlash = '\0';

       gtk_moz_embed_set_path(xpcomLocation);
#else
#ifdef HAVE_GECKO_1_9
	gtk_moz_embed_set_path (GECKO_HOME);
#else
	gtk_moz_embed_set_comp_path (GECKO_HOME);
#endif
#endif /* XPCOM_GLUE */

	gchar *profile_dir = g_build_filename (g_get_home_dir (),
					       ".evolution",
					       "mail",
					       "rss",
					       NULL);

	gtk_moz_embed_set_profile_path (profile_dir, "mozembed-rss");
	g_free (profile_dir);

	gtk_moz_embed_push_startup ();

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

#ifdef XPCOM_GLUE
	XPCOMGlueShutdown();
	NS_ShutdownXPCOM (nsnull);
#ifdef (EVOLUTION_VERSION < 22300)
	PR_ProcessExit (0);
#endif
#else
	gtk_moz_embed_pop_startup ();
#endif

#ifdef HAVE_GECKO_1_9
        NS_LogTerm ();
#endif
}
