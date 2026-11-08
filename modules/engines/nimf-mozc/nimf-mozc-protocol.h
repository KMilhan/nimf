/* SPDX-License-Identifier: LGPL-3.0-or-later */
#ifndef NIMF_MOZC_PROTOCOL_H
#define NIMF_MOZC_PROTOCOL_H

#include "nimf-mozc-sexp.h"

static inline const NimfMozcSexp *
nimf_mozc_get_candidate_window (const NimfMozcSexp *output)
{
  const NimfMozcSexp *window = nimf_mozc_sexp_lookup (output, "candidate-window");

  /* Older helpers expose the same window as "candidates". */
  return window ? window : nimf_mozc_sexp_lookup (output, "candidates");
}

#endif
