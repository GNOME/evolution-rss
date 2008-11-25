
#include "network.h"

GString*
fetch_blocking(const char *url, GSList *headers, GString *post,
                  NetStatusCallback cb, gpointer data,
                  GError **err) {

	gchar *scheme = NULL;
	GString *result = NULL;
	
	scheme = g_uri_parse_scheme(url);
	if (!g_ascii_strcasecmp(scheme, "file")) {
		gchar *fname = g_filename_from_uri(url, NULL, NULL);
		FILE *f = g_fopen(fname, "rb");
		g_free(fname);
		g_free(scheme);
	 	if (f == NULL)
                	goto error;	
		gchar *buf = g_new0 (gchar, 4096);
		result = g_string_new(NULL);
		while (fgets(buf, 4096, f) != NULL) {
			g_string_append_len(result, buf, strlen(buf));
		}
		fclose(f);
		return result;
	} else {
		g_free(scheme);
        	return net_post_blocking(url, NULL, post, cb, data, &err);
	}
error:
	g_print("error\n");
	g_set_error(err, NET_ERROR, NET_ERROR_GENERIC,
                                g_strerror(errno));
	return result;
}
