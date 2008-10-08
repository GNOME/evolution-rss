
#ifndef __PARSER_H__
#define __PARSER_H__

gchar *update_channel(const char *chn_name, char *url, char *main_date, GArray *item, GtkWidget *progress);

static char *layer_find (xmlNodePtr node, char *match, char *fail);
static char *layer_find_innerelement (xmlNodePtr node, char *match, char *el, char *fail);
static gchar *layer_find_innerhtml (xmlNodePtr node, char *match, char *submatch, char *fail);
xmlNodePtr layer_find_pos (xmlNodePtr node, char *match, char *submatch);
static char *layer_find_tag (xmlNodePtr node, char *match, char *fail);

#endif /*__RSS_H__*/

