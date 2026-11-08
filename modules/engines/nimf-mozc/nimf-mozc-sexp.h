/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-mozc-sexp.h
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

#ifndef __NIMF_MOZC_SEXP_H__
#define __NIMF_MOZC_SEXP_H__

#include <glib.h>

G_BEGIN_DECLS

/* A parsed mozc_emacs_helper reply.
 *
 * Dotted pairs follow Lisp rules: (a . (b c)) is the list (a b c).
 * So every alist entry is a list whose first item is its key. */
typedef struct _NimfMozcSexp NimfMozcSexp;

NimfMozcSexp       *nimf_mozc_sexp_parse    (const gchar        *text);
void                nimf_mozc_sexp_free     (NimfMozcSexp       *sexp);

guint               nimf_mozc_sexp_length   (const NimfMozcSexp *sexp);
const NimfMozcSexp *nimf_mozc_sexp_item     (const NimfMozcSexp *sexp,
                                             guint               index);
const NimfMozcSexp *nimf_mozc_sexp_lookup   (const NimfMozcSexp *sexp,
                                             const gchar        *key);
const gchar        *nimf_mozc_sexp_get_text (const NimfMozcSexp *sexp,
                                             const gchar        *key);
gint                nimf_mozc_sexp_get_int  (const NimfMozcSexp *sexp,
                                             const gchar        *key,
                                             gint                fallback);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NimfMozcSexp, nimf_mozc_sexp_free)

G_END_DECLS

#endif /* __NIMF_MOZC_SEXP_H__ */
