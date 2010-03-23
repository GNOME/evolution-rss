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
#include <glib.h>
#include <gtk/gtk.h>

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

#include <nsCOMPtr.h>
#include <nsICommandManager.h>
#include <nsIPrefService.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIServiceManager.h>
#include <nsServiceManagerUtils.h>
#include <nsIDOMMouseEvent.h>
#include <nsIDOMWindow.h>
#include <nsIContentViewer.h>
#include <nsIDocShell.h>
#include <nsIMarkupDocumentViewer.h>
#include <nspr.h>

extern "C" int rss_verbose_debug;
#include "debug.h"

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

extern "C" gboolean
gecko_prefs_set_int (const gchar *key, gint value)
{
	NS_ENSURE_TRUE (gPrefBranch, FALSE);

	return NS_SUCCEEDED(gPrefBranch->SetIntPref (key, value));
}

/*
 * Takes a pointer to a mouse event and returns the mouse
 * button number or -1 on error.
 */
extern "C"
gint gecko_get_mouse_event_button(gpointer event) {
	gint    button = 0;

	g_return_val_if_fail (event, -1);

	/* the following lines were found in the Galeon source */
	nsIDOMMouseEvent *aMouseEvent = (nsIDOMMouseEvent *) event;
	aMouseEvent->GetButton ((PRUint16 *) &button);

	/* for some reason we get different numbers on PPC, this fixes
	 * that up... -- MattA */
	if (button == 65536) {
		button = 1;
	} else if (button == 131072) {
		button = 2;
	}

	return button;
}

extern "C" void
gecko_set_zoom (GtkWidget *moz, gfloat zoom)
{
	nsCOMPtr<nsIWebBrowser>         mWebBrowser;
	nsCOMPtr<nsIDOMWindow>          mDOMWindow;

	gtk_moz_embed_get_nsIWebBrowser(
		(GtkMozEmbed *)moz, getter_AddRefs (mWebBrowser));
	if (NULL == mWebBrowser) {
		g_warning ("gecko_set_zoom(): Could not retrieve browser...");
		return;
	}
	mWebBrowser->GetContentDOMWindow(getter_AddRefs (mDOMWindow));
	if (NULL == mDOMWindow) {
		g_warning ("gecko_set_zoom(): Could not retrieve DOM window...");
		return;
	}
	mDOMWindow->SetTextZoom (zoom);
}

extern "C" gfloat
gecko_get_zoom (GtkWidget *embed)
{
	nsCOMPtr<nsIWebBrowser>         mWebBrowser;
	nsCOMPtr<nsIDOMWindow>          mDOMWindow;
	float zoom;

	gtk_moz_embed_get_nsIWebBrowser (
		(GtkMozEmbed *)embed, getter_AddRefs (mWebBrowser));
	if (NULL == mWebBrowser) {
		g_warning ("gecko_get_zoom(): Could not retrieve browser...");
		return 1.0;
	}
	mWebBrowser->GetContentDOMWindow (getter_AddRefs (mDOMWindow));
	if (NULL == mDOMWindow) {
		g_warning ("gecko_get_zoom(): Could not retrieve DOM window...");
		return 1.0;
	}
	mDOMWindow->GetTextZoom (&zoom);
	return zoom;
}

static nsresult
gecko_do_command (GtkMozEmbed *embed,
	const char  *command)
{
	nsCOMPtr<nsIWebBrowser>     webBrowser;
	nsCOMPtr<nsICommandManager> cmdManager;

	gtk_moz_embed_get_nsIWebBrowser (
		embed, getter_AddRefs (webBrowser));

	cmdManager = do_GetInterface (webBrowser);

	return cmdManager->DoCommand (command, nsnull, nsnull);
}

extern "C" void
gecko_copy_selection (GtkMozEmbed *embed)
{
	gecko_do_command (embed, "cmd_copy");
}

extern "C" void
gecko_select_all (GtkMozEmbed *embed)
{
	gecko_do_command (embed, "cmd_selectAll");
}


extern "C" gboolean
gecko_init (void)
{
	d("gecko_init()\n");
	nsresult rv;
#ifdef HAVE_GECKO_1_9
	NS_LogInit ();
#endif

#ifdef XPCOM_GLUE
	static const GREVersionRange greVersion = {
	"1.9a", PR_TRUE,
	"2.0", PR_TRUE,
	};
	char xpcomLocation[4096];
	d("init XPCOM_GLUE\n");
	rv = GRE_GetGREPathWithProperties(
		&greVersion, 1, nsnull, 0, xpcomLocation, 4096);
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
	d(g_print("doing old gecko init\n"));
#ifdef HAVE_GECKO_1_9
	gtk_moz_embed_set_path (GECKO_HOME);
#else
	gtk_moz_embed_set_comp_path (GECKO_HOME);
#endif
	d("end gecko init()\n");
#endif /* XPCOM_GLUE */

	d("load gecko prefs\n");
	gchar *profile_dir = g_build_filename (
				g_get_home_dir (),
				".evolution",
				"mail",
				"rss",
				NULL);

	gtk_moz_embed_set_profile_path (profile_dir, "mozembed-rss");
	g_free (profile_dir);

	d("embed push startup()\n");
	gtk_moz_embed_push_startup ();

	nsCOMPtr<nsIPrefService> prefService (
				do_GetService (NS_PREFSERVICE_CONTRACTID, &rv));
	NS_ENSURE_SUCCESS (rv, FALSE);

	rv = CallQueryInterface (prefService, &gPrefBranch);
	NS_ENSURE_SUCCESS (rv, FALSE);
	d("finished all gecko init\n");

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
#if (EVOLUTION_VERSION < 22300)
	PR_ProcessExit (0);
#endif
#else
	gtk_moz_embed_pop_startup ();
#endif

#ifdef HAVE_GECKO_1_9
	NS_LogTerm ();
#endif
}
