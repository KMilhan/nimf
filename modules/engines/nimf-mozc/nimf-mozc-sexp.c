/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-mozc-sexp.c
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

#include "nimf-mozc-sexp.h"
#include <string.h>

/* Replies nest about ten levels deep. This bound stops runaway recursion. */
#define NIMF_MOZC_SEXP_MAX_DEPTH 64

struct _NimfMozcSexp
{
  gchar     *text;  /* atom or string value, NULL for a list */
  GPtrArray *items; /* list items, NULL for an atom or string */
};

static NimfMozcSexp *parse_value (const gchar **cursor, guint depth);

static void
skip_spaces (const gchar **cursor)
{
  while (g_ascii_isspace (**cursor))
    (*cursor)++;
}

static gboolean
is_atom_end (gchar c)
{
  return c == '\0' || c == '(' || c == ')' || c == '"' || g_ascii_isspace (c);
}

static NimfMozcSexp *
sexp_new_list (void)
{
  NimfMozcSexp *sexp = g_new0 (NimfMozcSexp, 1);

  sexp->items = g_ptr_array_new_with_free_func
                  ((GDestroyNotify) nimf_mozc_sexp_free);

  return sexp;
}

static NimfMozcSexp *
sexp_new_text (gchar *text)
{
  NimfMozcSexp *sexp = g_new0 (NimfMozcSexp, 1);

  sexp->text = text;

  return sexp;
}

static NimfMozcSexp *
parse_string (const gchar **cursor)
{
  GString     *value = g_string_new (NULL);
  const gchar *p     = *cursor + 1;

  for (; *p != '\0' && *p != '"'; p++)
  {
    if (*p == '\\' && p[1] != '\0')
      p++;

    g_string_append_c (value, *p);
  }

  if (*p != '"')
  {
    g_string_free (value, TRUE);
    return NULL;
  }

  *cursor = p + 1;

  return sexp_new_text (g_string_free (value, FALSE));
}

static NimfMozcSexp *
parse_atom (const gchar **cursor)
{
  const gchar *start = *cursor;

  while (!is_atom_end (**cursor))
    (*cursor)++;

  if (*cursor == start)
    return NULL;

  return sexp_new_text (g_strndup (start, *cursor - start));
}

static gboolean
is_dot (const gchar *cursor)
{
  return cursor[0] == '.' && is_atom_end (cursor[1]);
}

/* Appends the value after a dot. A list value is spliced into the parent. */
static gboolean
parse_dotted_tail (const gchar **cursor, NimfMozcSexp *list, guint depth)
{
  NimfMozcSexp *tail;

  (*cursor)++;
  tail = parse_value (cursor, depth);

  if (tail == NULL)
    return FALSE;

  if (tail->items == NULL)
  {
    g_ptr_array_add (list->items, tail);
    return TRUE;
  }

  while (tail->items->len > 0)
    g_ptr_array_add (list->items, g_ptr_array_steal_index (tail->items, 0));

  nimf_mozc_sexp_free (tail);

  return TRUE;
}

static NimfMozcSexp *
parse_list (const gchar **cursor, guint depth)
{
  NimfMozcSexp *list = sexp_new_list ();

  (*cursor)++;

  while (TRUE)
  {
    skip_spaces (cursor);

    if (**cursor == ')')
    {
      (*cursor)++;
      return list;
    }

    if (is_dot (*cursor))
    {
      if (!parse_dotted_tail (cursor, list, depth))
        break;

      skip_spaces (cursor);

      if (**cursor != ')')
        break;

      continue;
    }

    NimfMozcSexp *item = parse_value (cursor, depth);

    if (item == NULL)
      break;

    g_ptr_array_add (list->items, item);
  }

  nimf_mozc_sexp_free (list);

  return NULL;
}

static NimfMozcSexp *
parse_value (const gchar **cursor, guint depth)
{
  if (depth >= NIMF_MOZC_SEXP_MAX_DEPTH)
    return NULL;

  skip_spaces (cursor);

  switch (**cursor)
  {
    case '(':
      return parse_list (cursor, depth + 1);
    case '"':
      return parse_string (cursor);
    default:
      return parse_atom (cursor);
  }
}

/**
 * nimf_mozc_sexp_parse:
 * @text: one reply line
 *
 * Returns: (nullable): the parsed value, or %NULL when @text is malformed
 */
NimfMozcSexp *
nimf_mozc_sexp_parse (const gchar *text)
{
  const gchar  *cursor = text;
  NimfMozcSexp *sexp   = parse_value (&cursor, 0);

  if (sexp == NULL)
    return NULL;

  skip_spaces (&cursor);

  if (*cursor != '\0')
  {
    nimf_mozc_sexp_free (sexp);
    return NULL;
  }

  return sexp;
}

void
nimf_mozc_sexp_free (NimfMozcSexp *sexp)
{
  if (sexp == NULL)
    return;

  if (sexp->items)
    g_ptr_array_unref (sexp->items);

  g_free (sexp->text);
  g_free (sexp);
}

guint
nimf_mozc_sexp_length (const NimfMozcSexp *sexp)
{
  return sexp && sexp->items ? sexp->items->len : 0;
}

const NimfMozcSexp *
nimf_mozc_sexp_item (const NimfMozcSexp *sexp, guint index)
{
  if (index >= nimf_mozc_sexp_length (sexp))
    return NULL;

  return g_ptr_array_index (sexp->items, index);
}

/**
 * nimf_mozc_sexp_lookup:
 * @sexp: (nullable): an alist, or an alist entry holding a nested alist
 * @key: the entry key
 *
 * Returns: (nullable): the first entry whose key is @key
 */
const NimfMozcSexp *
nimf_mozc_sexp_lookup (const NimfMozcSexp *sexp, const gchar *key)
{
  guint n = nimf_mozc_sexp_length (sexp);
  guint i;

  for (i = 0; i < n; i++)
  {
    const NimfMozcSexp *entry = nimf_mozc_sexp_item (sexp, i);
    const NimfMozcSexp *head  = nimf_mozc_sexp_item (entry, 0);

    if (head && head->text && strcmp (head->text, key) == 0)
      return entry;
  }

  return NULL;
}

/**
 * nimf_mozc_sexp_get_text:
 * @sexp: (nullable): an alist
 * @key: the entry key
 *
 * Returns: (nullable): the atom or string stored under @key
 */
const gchar *
nimf_mozc_sexp_get_text (const NimfMozcSexp *sexp, const gchar *key)
{
  const NimfMozcSexp *value;

  value = nimf_mozc_sexp_item (nimf_mozc_sexp_lookup (sexp, key), 1);

  return value ? value->text : NULL;
}

gint
nimf_mozc_sexp_get_int (const NimfMozcSexp *sexp,
                        const gchar        *key,
                        gint                fallback)
{
  const gchar *text = nimf_mozc_sexp_get_text (sexp, key);
  gchar       *end;
  gint64       value;

  if (text == NULL)
    return fallback;

  value = g_ascii_strtoll (text, &end, 10);

  return end != text && *end == '\0' ? (gint) value : fallback;
}
