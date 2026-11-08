/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-mozc.c
 * This file is part of Nimf.
 *
 * Nimf is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nimf is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program;  If not, see <http://www.gnu.org/licenses/>.
 */

#include <nimf.h>
#include <string.h>
#include "nimf-mozc-helper.h"
#include "nimf-mozc-key.h"
#include "nimf-mozc-protocol.h"

#define NIMF_TYPE_MOZC             (nimf_mozc_get_type ())
#define NIMF_MOZC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), NIMF_TYPE_MOZC, NimfMozc))
#define NIMF_MOZC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), NIMF_TYPE_MOZC, NimfMozcClass))
#define NIMF_IS_MOZC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NIMF_TYPE_MOZC))
#define NIMF_IS_MOZC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), NIMF_TYPE_MOZC))
#define NIMF_MOZC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), NIMF_TYPE_MOZC, NimfMozcClass))

#define NIMF_MOZC_HELPER_PROGRAM   "mozc_emacs_helper"
/* A reply takes 1 to 15 ms, or about 200 ms while mozc_server starts.
 * filter_event blocks the whole server, so the limit stays short. */
#define NIMF_MOZC_TIMEOUT_MS       3000
#define NIMF_MOZC_RETRY_DELAY_MS   30000
#define NIMF_MOZC_DEFAULT_PAGE_SIZE 9
#define NIMF_MOZC_MAX_SHORTCUTS    9

typedef struct _NimfMozc      NimfMozc;
typedef struct _NimfMozcClass NimfMozcClass;

struct _NimfMozc
{
  NimfEngine parent_instance;

  NimfCandidatable *candidatable;
  NimfMozcHelper   *helper;
  GString          *preedit;
  NimfPreeditAttr **preedit_attrs;
  NimfPreeditState  preedit_state;
  gint              cursor_pos;
  gint              current_page;
  gint              n_pages;
  gboolean          has_candidates;
};

struct _NimfMozcClass
{
  /*< private >*/
  NimfEngineClass parent_class;
};

G_DEFINE_DYNAMIC_TYPE (NimfMozc, nimf_mozc, NIMF_TYPE_ENGINE);

static void
nimf_mozc_set_preedit (NimfEngine       *engine,
                       NimfServiceIC    *target,
                       const gchar      *text,
                       NimfPreeditAttr **attrs,
                       gint              cursor_pos)
{
  NimfMozc *mozc       = NIMF_MOZC (engine);
  gboolean  had_text   = mozc->preedit->len > 0;

  g_string_assign (mozc->preedit, text);
  nimf_preedit_attr_freev (mozc->preedit_attrs);
  mozc->preedit_attrs = attrs;
  mozc->cursor_pos    = cursor_pos;

  if (mozc->preedit_state == NIMF_PREEDIT_STATE_END && mozc->preedit->len > 0)
  {
    mozc->preedit_state = NIMF_PREEDIT_STATE_START;
    nimf_engine_emit_preedit_start (engine, target);
  }

  if (had_text || mozc->preedit->len > 0)
    nimf_engine_emit_preedit_changed (engine, target, mozc->preedit->str,
                                      mozc->preedit_attrs, cursor_pos);

  if (mozc->preedit_state == NIMF_PREEDIT_STATE_START && mozc->preedit->len == 0)
  {
    mozc->preedit_state = NIMF_PREEDIT_STATE_END;
    nimf_engine_emit_preedit_end (engine, target);
  }
}

static NimfPreeditAttr **
nimf_mozc_attrs_new_empty (void)
{
  return g_new0 (NimfPreeditAttr *, 1);
}

static void
nimf_mozc_apply_preedit (NimfEngine         *engine,
                         NimfServiceIC      *target,
                         const NimfMozcSexp *output)
{
  const NimfMozcSexp *preedit  = nimf_mozc_sexp_lookup (output, "preedit");
  const NimfMozcSexp *segments = nimf_mozc_sexp_lookup (preedit, "segment");
  GString   *text  = g_string_new (NULL);
  GPtrArray *attrs = g_ptr_array_new ();
  guint      offset = 0;
  guint      i;

  /* Item 0 of an alist entry is its key. */
  for (i = 1; i < nimf_mozc_sexp_length (segments); i++)
  {
    const NimfMozcSexp *segment = nimf_mozc_sexp_item (segments, i);
    const gchar *value      = nimf_mozc_sexp_get_text (segment, "value");
    const gchar *annotation = nimf_mozc_sexp_get_text (segment, "annotation");
    guint        length;

    if (value == NULL)
      continue;

    length = g_utf8_strlen (value, -1);
    g_string_append (text, value);
    g_ptr_array_add (attrs, nimf_preedit_attr_new
                     (g_strcmp0 (annotation, "highlight") == 0 ?
                        NIMF_PREEDIT_ATTR_HIGHLIGHT : NIMF_PREEDIT_ATTR_UNDERLINE,
                      offset, offset + length));
    offset += length;
  }

  g_ptr_array_add (attrs, NULL);
  nimf_mozc_set_preedit (engine, target, text->str,
                         (NimfPreeditAttr **) g_ptr_array_free (attrs, FALSE),
                         nimf_mozc_sexp_get_int (preedit, "cursor", 0));
  g_string_free (text, TRUE);
}

static void
nimf_mozc_fill_candidates (NimfMozc           *mozc,
                           NimfServiceIC      *target,
                           const NimfMozcSexp *candidates)
{
  const NimfMozcSexp *list  = nimf_mozc_sexp_lookup (candidates, "candidate");
  const NimfMozcSexp *first = nimf_mozc_sexp_item (list, 1);
  gint  page_size   = nimf_mozc_sexp_get_int (candidates, "page-size",
                                              NIMF_MOZC_DEFAULT_PAGE_SIZE);
  gint  size        = nimf_mozc_sexp_get_int (candidates, "size", 0);
  gint  focused     = nimf_mozc_sexp_get_int (candidates, "focused-index", 0);
  gint  first_index = nimf_mozc_sexp_get_int (first, "index", 0);
  guint i;

  if (page_size <= 0)
    page_size = NIMF_MOZC_DEFAULT_PAGE_SIZE;

  for (i = 1; i < nimf_mozc_sexp_length (list); i++)
  {
    const NimfMozcSexp *candidate  = nimf_mozc_sexp_item (list, i);
    const NimfMozcSexp *annotation = nimf_mozc_sexp_lookup (candidate, "annotation");

    nimf_candidatable_append (mozc->candidatable,
                              nimf_mozc_sexp_get_text (candidate, "value"),
                              nimf_mozc_sexp_get_text (annotation, "description"));
  }

  mozc->current_page = first_index / page_size + 1;
  mozc->n_pages      = (size + page_size - 1) / page_size;

  nimf_candidatable_select_item_by_index_in_page (mozc->candidatable,
                                                  focused - first_index);
  nimf_candidatable_set_page_values (mozc->candidatable, target,
                                     mozc->current_page, mozc->n_pages,
                                     page_size);
}

/* Suggestions shown while typing have no focused-index and no shortcut keys,
 * so a click could not select them. Only selectable lists reach the window. */
static void
nimf_mozc_apply_candidates (NimfMozc           *mozc,
                            NimfServiceIC      *target,
                            const NimfMozcSexp *output)
{
  const NimfMozcSexp *candidates = nimf_mozc_get_candidate_window (output);
  gboolean use_preedit  = nimf_service_ic_get_use_preedit (target);
  gboolean needs_window;

  mozc->has_candidates =
    nimf_mozc_sexp_lookup (candidates, "focused-index") != NULL;

  nimf_candidatable_clear (mozc->candidatable, target);

  if (mozc->has_candidates)
    nimf_mozc_fill_candidates (mozc, target, candidates);

  /* Without preedit support the window entry is the only composition view. */
  needs_window = mozc->has_candidates || (!use_preedit && mozc->preedit->len > 0);

  if (!use_preedit)
    nimf_candidatable_set_auxiliary_text (mozc->candidatable, mozc->preedit->str,
                                          mozc->cursor_pos);

  if (!needs_window)
    nimf_candidatable_hide (mozc->candidatable);
  else if (!nimf_candidatable_is_visible (mozc->candidatable))
    nimf_candidatable_show (mozc->candidatable, target, !use_preedit);
}

static void
nimf_mozc_apply_output (NimfEngine         *engine,
                        NimfServiceIC      *target,
                        const NimfMozcSexp *output)
{
  const NimfMozcSexp *result = nimf_mozc_sexp_lookup (output, "result");
  const gchar        *value  = nimf_mozc_sexp_get_text (result, "value");

  if (value)
    nimf_engine_emit_commit (engine, target, value);

  nimf_mozc_apply_preedit (engine, target, output);
  nimf_mozc_apply_candidates (NIMF_MOZC (engine), target, output);
}

/* The helper lost its state. Keep what the user sees instead of erasing it. */
static void
nimf_mozc_drop_composition (NimfEngine *engine, NimfServiceIC *target)
{
  NimfMozc *mozc = NIMF_MOZC (engine);

  if (mozc->preedit->len > 0)
  {
    gchar *text = g_strdup (mozc->preedit->str);

    nimf_mozc_set_preedit (engine, target, "", nimf_mozc_attrs_new_empty (), 0);
    nimf_engine_emit_commit (engine, target, text);
    g_free (text);
  }

  mozc->has_candidates = FALSE;
  nimf_candidatable_hide (mozc->candidatable);
}

/* Returns TRUE when mozc consumed the key. */
static gboolean
nimf_mozc_send_key (NimfEngine    *engine,
                    NimfServiceIC *target,
                    const gchar   *tokens)
{
  NimfMozcSexp       *reply;
  const NimfMozcSexp *output;
  gboolean            consumed;

  reply = nimf_mozc_helper_send_key (NIMF_MOZC (engine)->helper, tokens);

  if (reply == NULL)
  {
    nimf_mozc_drop_composition (engine, target);
    return FALSE;
  }

  output   = nimf_mozc_sexp_lookup (reply, "output");
  consumed = g_strcmp0 (nimf_mozc_sexp_get_text (output, "consumed"), "t") == 0;
  nimf_mozc_apply_output (engine, target, output);
  nimf_mozc_sexp_free (reply);

  return consumed;
}

static gboolean
nimf_mozc_filter_event (NimfEngine    *engine,
                        NimfServiceIC *target,
                        NimfEvent     *event)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  gchar   *tokens;
  gboolean consumed;

  if (event->key.type == NIMF_EVENT_KEY_RELEASE)
    return FALSE;

  tokens = nimf_mozc_key_to_tokens (event->key.keyval, event->key.state);

  if (tokens == NULL)
    return FALSE;

  consumed = nimf_mozc_send_key (engine, target, tokens);
  g_free (tokens);

  return consumed;
}

/* The helper has no reset command, so Enter commits the composition. */
static void
nimf_mozc_reset (NimfEngine *engine, NimfServiceIC *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfMozc *mozc = NIMF_MOZC (engine);

  if (mozc->preedit->len > 0)
    nimf_mozc_send_key (engine, target, "enter");

  nimf_candidatable_hide (mozc->candidatable);
}

static void
nimf_mozc_focus_in (NimfEngine *engine, NimfServiceIC *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
}

static void
nimf_mozc_focus_out (NimfEngine *engine, NimfServiceIC *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  nimf_mozc_reset (engine, target);
}

static gboolean
nimf_mozc_page_up (NimfEngine *engine, NimfServiceIC *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return nimf_mozc_send_key (engine, target, "pageup");
}

static gboolean
nimf_mozc_page_down (NimfEngine *engine, NimfServiceIC *target)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return nimf_mozc_send_key (engine, target, "pagedown");
}

/* The helper has no select command. A digit selects within the current page. */
static void
on_candidate_clicked (NimfEngine    *engine,
                      NimfServiceIC *target,
                      gchar         *text,
                      gint           index)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  gchar *tokens;

  if (!NIMF_MOZC (engine)->has_candidates ||
      index < 0 || index >= NIMF_MOZC_MAX_SHORTCUTS)
    return;

  tokens = g_strdup_printf ("%d", '1' + index);
  nimf_mozc_send_key (engine, target, tokens);
  g_free (tokens);
}

static void
on_candidate_scrolled (NimfEngine    *engine,
                       NimfServiceIC *target,
                       gdouble        value)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfMozc *mozc  = NIMF_MOZC (engine);
  gint      delta = (gint) value - mozc->current_page;
  gint      i;

  for (i = 0; i < ABS (delta); i++)
    if (!nimf_mozc_send_key (engine, target, delta > 0 ? "pagedown" : "pageup"))
      break;
}

static const gchar *
nimf_mozc_get_id (NimfEngine *engine)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return "nimf-mozc";
}

static const gchar *
nimf_mozc_get_icon_name (NimfEngine *engine)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return "nimf-mozc";
}

static void
nimf_mozc_init (NimfMozc *mozc)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  mozc->helper = nimf_mozc_helper_new (NIMF_MOZC_HELPER_PROGRAM,
                                       NIMF_MOZC_TIMEOUT_MS,
                                       NIMF_MOZC_RETRY_DELAY_MS);
  mozc->preedit       = g_string_new (NULL);
  mozc->preedit_attrs = nimf_mozc_attrs_new_empty ();
  mozc->preedit_state = NIMF_PREEDIT_STATE_END;
  mozc->current_page  = 1;
  mozc->n_pages       = 1;
}

static void
nimf_mozc_finalize (GObject *object)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfMozc *mozc = NIMF_MOZC (object);

  nimf_mozc_helper_free (mozc->helper);
  g_string_free (mozc->preedit, TRUE);
  nimf_preedit_attr_freev (mozc->preedit_attrs);

  G_OBJECT_CLASS (nimf_mozc_parent_class)->finalize (object);
}

static void
nimf_mozc_constructed (GObject *object)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  NimfMozc *mozc = NIMF_MOZC (object);

  mozc->candidatable = nimf_engine_get_candidatable (NIMF_ENGINE (mozc));
}

static void
nimf_mozc_class_init (NimfMozcClass *class)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  GObjectClass    *object_class = G_OBJECT_CLASS (class);
  NimfEngineClass *engine_class = NIMF_ENGINE_CLASS (class);

  engine_class->filter_event        = nimf_mozc_filter_event;
  engine_class->reset               = nimf_mozc_reset;
  engine_class->focus_in            = nimf_mozc_focus_in;
  engine_class->focus_out           = nimf_mozc_focus_out;

  engine_class->candidate_page_up   = nimf_mozc_page_up;
  engine_class->candidate_page_down = nimf_mozc_page_down;
  engine_class->candidate_clicked   = on_candidate_clicked;
  engine_class->candidate_scrolled  = on_candidate_scrolled;

  engine_class->get_id              = nimf_mozc_get_id;
  engine_class->get_icon_name       = nimf_mozc_get_icon_name;

  object_class->constructed = nimf_mozc_constructed;
  object_class->finalize    = nimf_mozc_finalize;
}

static void
nimf_mozc_class_finalize (NimfMozcClass *class)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);
}

void
module_register_type (GTypeModule *type_module)
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  nimf_mozc_register_type (type_module);
}

GType
module_get_type ()
{
  g_debug (G_STRLOC ": %s", G_STRFUNC);

  return nimf_mozc_get_type ();
}
