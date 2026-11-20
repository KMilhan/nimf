#include <ibus.h>
#include <nimf.h>

typedef struct
{
  IBusEngine parent;
  NimfIM *im;
} NimfIBusEngine;

typedef IBusEngineClass NimfIBusEngineClass;

G_DEFINE_TYPE (NimfIBusEngine, nimf_ibus_engine, IBUS_TYPE_ENGINE)

static void
commit (NimfIM *im, const gchar *text, IBusEngine *engine)
{
  (void) im;
  ibus_engine_commit_text (engine, ibus_text_new_from_string (text));
}

static void
preedit_changed (NimfIM *im, IBusEngine *engine)
{
  g_autofree gchar *text = NULL;
  NimfPreeditAttr **attrs = NULL;
  gint cursor = 0;
  nimf_im_get_preedit_string (im, &text, &attrs, &cursor);
  IBusText *preedit = ibus_text_new_from_string (text);
  for (guint i = 0; attrs && attrs[i]; i++)
    if (attrs[i]->type == NIMF_PREEDIT_ATTR_UNDERLINE)
      ibus_text_append_attribute (preedit, IBUS_ATTR_TYPE_UNDERLINE,
                                  IBUS_ATTR_UNDERLINE_SINGLE,
                                  attrs[i]->start_index, attrs[i]->end_index);
  ibus_engine_update_preedit_text (engine, preedit, cursor, *text != '\0');
  nimf_preedit_attr_freev (attrs);
}

static void
preedit_end (NimfIM *im, IBusEngine *engine)
{
  (void) im;
  ibus_engine_hide_preedit_text (engine);
}

static gboolean
retrieve_surrounding (NimfIM *im, IBusEngine *engine)
{
  if (!(engine->client_capabilities & IBUS_CAP_SURROUNDING_TEXT))
    return FALSE;

  IBusText *text = NULL;
  guint cursor = 0, anchor = 0;
  ibus_engine_get_surrounding_text (engine, &text, &cursor, &anchor);
  if (!text)
    return FALSE;

  nimf_im_set_surrounding (im, ibus_text_get_text (text), -1, cursor);
  return TRUE;
}

static gboolean
delete_surrounding (NimfIM *im, gint offset, gint count, IBusEngine *engine)
{
  (void) im;
  if (!(engine->client_capabilities & IBUS_CAP_SURROUNDING_TEXT) || count < 0)
    return FALSE;

  ibus_engine_delete_surrounding_text (engine, offset, count);
  return TRUE;
}

static gboolean
process_key_event (IBusEngine *engine, guint keyval, guint keycode, guint state)
{
  NimfIBusEngine *self = (NimfIBusEngine *) engine;
  NimfEvent event = {
    .key = {
      .type = (state & IBUS_RELEASE_MASK) ? NIMF_EVENT_KEY_RELEASE : NIMF_EVENT_KEY_PRESS,
      .state = state & NIMF_MODIFIER_MASK,
      .keyval = keyval,
      .hardware_keycode = keycode + 8
    }
  };
  return nimf_im_filter_event (self->im, &event);
}

static void
enable (IBusEngine *engine)
{
  ibus_engine_get_surrounding_text (engine, NULL, NULL, NULL);
}

static void
focus_in (IBusEngine *engine)
{
  nimf_im_focus_in (((NimfIBusEngine *) engine)->im);
}

static void
focus_out (IBusEngine *engine)
{
  nimf_im_focus_out (((NimfIBusEngine *) engine)->im);
}

static void
reset (IBusEngine *engine)
{
  nimf_im_reset (((NimfIBusEngine *) engine)->im);
}

static void
dispose (GObject *object)
{
  NimfIBusEngine *self = (NimfIBusEngine *) object;
  g_clear_object (&self->im);
  G_OBJECT_CLASS (nimf_ibus_engine_parent_class)->dispose (object);
}

static void
nimf_ibus_engine_class_init (NimfIBusEngineClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = dispose;
  klass->process_key_event = process_key_event;
  klass->enable = enable;
  klass->focus_in = focus_in;
  klass->focus_out = focus_out;
  klass->reset = reset;
}

static void
nimf_ibus_engine_init (NimfIBusEngine *self)
{
  self->im = nimf_im_new ();
  g_signal_connect (self->im, "commit", G_CALLBACK (commit), self);
  g_signal_connect (self->im, "preedit-changed", G_CALLBACK (preedit_changed), self);
  g_signal_connect (self->im, "preedit-end", G_CALLBACK (preedit_end), self);
  g_signal_connect (self->im, "retrieve-surrounding", G_CALLBACK (retrieve_surrounding), self);
  g_signal_connect (self->im, "delete-surrounding", G_CALLBACK (delete_surrounding), self);
}

static void
bus_disconnected (IBusBus *bus, gpointer data)
{
  (void) bus;
  (void) data;
  ibus_quit ();
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (IBusBus, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (IBusFactory, g_object_unref)

int
main (void)
{
  ibus_init ();
  g_autoptr(IBusBus) bus = ibus_bus_new ();
  if (!ibus_bus_is_connected (bus))
    return 1;

  g_autoptr(IBusFactory) factory = ibus_factory_new (ibus_bus_get_connection (bus));
  ibus_factory_add_engine (factory, "nimf", nimf_ibus_engine_get_type ());
  if (ibus_bus_request_name (bus, "org.freedesktop.IBus.Nimf", 0) != 1)
    return 1;

  g_signal_connect (bus, "disconnected", G_CALLBACK (bus_disconnected), NULL);
  ibus_main ();
  return 0;
}
