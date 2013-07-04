
#ifndef E_MAIL_PART_RSS_H
#define E_MAIL_PART_RSS_H

#include <em-format/e-mail-part.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PART_RSS \
        (e_mail_part_rss_get_type ())
#define E_MAIL_PART_RSS(obj) \
        (G_TYPE_CHECK_INSTANCE_CAST \
        ((obj), E_TYPE_MAIL_PART_RSS, EMailPartRSS))
#define E_MAIL_PART_RSS_CLASS(cls) \
        (G_TYPE_CHECK_CLASS_CAST \
        ((cls), E_TYPE_MAIL_PART_RSS, EMailPartRSSClass))
#define E_IS_MAIL_PART_RSS(obj) \
        (G_TYPE_CHECK_INSTANCE_TYPE \
        ((obj), E_TYPE_MAIL_PART_RSS))
#define E_IS_MAIL_PART_RSS_CLASS(cls) \
        (G_TYPE_CHECK_CLASS_TYPE \
        ((cls), E_TYPE_MAIL_PART_RSS))
#define E_MAIL_PART_RSS_GET_CLASS(obj) \
        (G_TYPE_INSTANCE_GET_CLASS \
        ((obj), E_TYPE_MAIL_PART_RSS, EMailPartRSSClass))

G_BEGIN_DECLS

typedef struct _EMailPartRSS EMailPartRSS;
typedef struct _EMailPartRSSClass EMailPartRSSClass;
typedef struct _EMailPartRSSPrivate EMailPartRSSPrivate;

struct _EMailPartRSS {
        EMailPart parent;
        EMailPartRSSPrivate *priv;

       // gchar *filename;
      //  GstElement *playbin;
        //gulong      bus_id;
      //  GstState    target_state;
    //    GtkWidget  *play_button;
  //      GtkWidget  *pause_button;
//        GtkWidget  *stop_button;
};

struct _EMailPartRSSClass {
        EMailPartClass parent_class;
};

GType           e_mail_part_rss_get_type      (void) G_GNUC_CONST;
void            e_mail_part_rss_type_register (GTypeModule *type_module);
EMailPart *     e_mail_part_rss_new           (CamelMimePart *mime_part,
                                                 const gchar *id);

G_END_DECLS

#endif /* E_MAIL_PART_RSS_H */

