/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-mozc-key.c
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

#include "nimf-mozc-key.h"
#include <nimf.h>

typedef struct {
  guint32      keyval;
  const gchar *token;
} SpecialKey;

static const SpecialKey special_keys[] = {
  {NIMF_KEY_space,     "space"},
  {NIMF_KEY_Return,    "enter"},
  {NIMF_KEY_KP_Enter,  "enter"},
  {NIMF_KEY_Tab,       "tab"},
  {NIMF_KEY_BackSpace, "backspace"},
  {NIMF_KEY_Delete,    "delete"},
  {NIMF_KEY_Insert,    "insert"},
  {NIMF_KEY_Escape,    "escape"},
  {NIMF_KEY_Home,      "home"},
  {NIMF_KEY_End,       "end"},
  {NIMF_KEY_Left,      "left"},
  {NIMF_KEY_Right,     "right"},
  {NIMF_KEY_Up,        "up"},
  {NIMF_KEY_Down,      "down"},
  {NIMF_KEY_Page_Up,   "pageup"},
  {NIMF_KEY_Page_Down, "pagedown"}
};

static gchar *
special_key_token (guint32 keyval)
{
  guint i;

  if (keyval >= NIMF_KEY_F1 && keyval <= NIMF_KEY_F12)
    return g_strdup_printf ("f%u", keyval - NIMF_KEY_F1 + 1);

  for (i = 0; i < G_N_ELEMENTS (special_keys); i++)
    if (special_keys[i].keyval == keyval)
      return g_strdup (special_keys[i].token);

  return NULL;
}

/**
 * nimf_mozc_key_to_tokens:
 * @keyval: the key symbol
 * @state: the modifier mask
 *
 * Builds the SendKey arguments for a key press. A printable key goes as its
 * keyval because Shift is already applied there: mozc ignores "shift 97" but
 * reads 65 as "A".
 *
 * Returns: (nullable): the tokens, or %NULL when mozc cannot take this key
 */
gchar *
nimf_mozc_key_to_tokens (guint32 keyval, guint32 state)
{
  GString *tokens;
  gchar   *key;
  gboolean is_printable = keyval > NIMF_KEY_space && keyval <= NIMF_KEY_asciitilde;

  if (state & (NIMF_SUPER_MASK | NIMF_HYPER_MASK | NIMF_META_MASK))
    return NULL;

  if (keyval == NIMF_KEY_ISO_Left_Tab)
  {
    keyval = NIMF_KEY_Tab;
    state |= NIMF_SHIFT_MASK;
  }

  key = is_printable ? g_strdup_printf ("%u", keyval)
                     : special_key_token (keyval);

  if (key == NULL)
    return NULL;

  tokens = g_string_new (NULL);

  if (state & NIMF_CONTROL_MASK)
    g_string_append (tokens, "ctrl ");

  if (state & NIMF_MOD1_MASK)
    g_string_append (tokens, "alt ");

  if ((state & NIMF_SHIFT_MASK) && !is_printable)
    g_string_append (tokens, "shift ");

  g_string_append (tokens, key);
  g_free (key);

  return g_string_free (tokens, FALSE);
}
