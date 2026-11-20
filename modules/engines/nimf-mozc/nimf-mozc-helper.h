/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-mozc-helper.h
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

#ifndef __NIMF_MOZC_HELPER_H__
#define __NIMF_MOZC_HELPER_H__

#include <glib.h>
#include "nimf-mozc-sexp.h"

G_BEGIN_DECLS

#define NIMF_MOZC_HELPER_ERROR (nimf_mozc_helper_error_quark ())

/**
 * NimfMozcHelperError:
 * @NIMF_MOZC_HELPER_ERROR_NOT_FOUND: the helper program is not installed
 * @NIMF_MOZC_HELPER_ERROR_FAILED: the helper failed; a restart waits for the
 *   retry delay, so Japanese input stays off until then
 * @NIMF_MOZC_HELPER_ERROR_LOST: the helper exited; the next call restarts it
 */
typedef enum
{
  NIMF_MOZC_HELPER_ERROR_NOT_FOUND,
  NIMF_MOZC_HELPER_ERROR_FAILED,
  NIMF_MOZC_HELPER_ERROR_LOST
} NimfMozcHelperError;

/* Owns one mozc_emacs_helper child process and its mozc session. */
typedef struct _NimfMozcHelper NimfMozcHelper;

GQuark          nimf_mozc_helper_error_quark (void);
NimfMozcHelper *nimf_mozc_helper_new      (const gchar    *program,
                                           gint            timeout_ms,
                                           gint            retry_delay_ms);
void            nimf_mozc_helper_free     (NimfMozcHelper *helper);
NimfMozcSexp   *nimf_mozc_helper_send_key (NimfMozcHelper *helper,
                                           const gchar    *tokens,
                                           GError        **error);

G_END_DECLS

#endif /* __NIMF_MOZC_HELPER_H__ */
