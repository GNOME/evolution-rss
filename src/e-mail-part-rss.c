
#include "e-mail-part-rss.h"


#define E_MAIL_PART_RSS_GET_PRIVATE(obj) \
        (G_TYPE_INSTANCE_GET_PRIVATE \
        ((obj), E_TYPE_MAIL_PART_RSS, EMailPartRSSPrivate))

struct _EMailPartRSSPrivate {
        gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
        EMailPartRSS,
        e_mail_part_rss,
        E_TYPE_MAIL_PART)

static void
mail_part_rss_dispose (GObject *object)
{
        /*EMailPartAudio *part = E_MAIL_PART_AUDIO (object);

        if (part->bus_id > 0) {
                g_source_remove (part->bus_id);
                part->bus_id = 0;
        }

        if (part->playbin != NULL) {
                gst_element_set_state (part->playbin, GST_STATE_NULL);
                gst_object_unref (part->playbin);
                part->playbin = NULL;
        }
        g_clear_object (&part->play_button);
        g_clear_object (&part->pause_button);
        g_clear_object (&part->stop_button);*/

        /* Chain up to parent's dispose() method. */
        G_OBJECT_CLASS (e_mail_part_rss_parent_class)->dispose (object);
}

static void
mail_part_rss_finalize (GObject *object)
{
	//EMailPartAudio *part = E_MAIL_PART_AUDIO (object);

	//if (part->filename != NULL) {
	//        g_unlink (part->filename);
	//        g_free (part->filename);
	//}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_part_rss_parent_class)->finalize (object);
}

static void
mail_part_rss_constructed (GObject *object)
{
        EMailPart *part;
        CamelMimePart *mime_part;
        CamelContentType *content_type;
        gchar *mime_type;

        part = E_MAIL_PART (object);

        /* Chain up to parent's constructed() method. */
        G_OBJECT_CLASS (e_mail_part_rss_parent_class)->constructed (object);

        //e_mail_part_set_is_attachment (part, TRUE);
        mime_part = e_mail_part_ref_mime_part (part);

        content_type = camel_mime_part_get_content_type (mime_part);
        mime_type = camel_content_type_simple (content_type);
        e_mail_part_set_mime_type (part, mime_type);
        g_free (mime_type);

        g_object_unref (mime_part);
}

static void
e_mail_part_rss_class_init (EMailPartRSSClass *class)
{
        GObjectClass *object_class;

        g_type_class_add_private (class, sizeof (EMailPartRSSPrivate));

        object_class = G_OBJECT_CLASS (class);
        object_class->dispose = mail_part_rss_dispose;
        object_class->finalize = mail_part_rss_finalize;
        object_class->constructed = mail_part_rss_constructed;
}

static void
e_mail_part_rss_class_finalize (EMailPartRSSClass *class)
{
}

static void
e_mail_part_rss_init (EMailPartRSS *part)
{
        part->priv = E_MAIL_PART_RSS_GET_PRIVATE (part);
}

void
e_mail_part_rss_type_register (GTypeModule *type_module)
{
        /* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
         *     function, so we have to wrap it with a public function in
         *     order to register types from a separate compilation unit. */
        e_mail_part_rss_register_type (type_module);
}

EMailPart *
e_mail_part_rss_new (CamelMimePart *mime_part,
			const gchar *id)
{
	g_return_val_if_fail (id != NULL, NULL);

	return g_object_new (
		E_TYPE_MAIL_PART_RSS,
		"id", id, "mime-part", mime_part, NULL);
}

