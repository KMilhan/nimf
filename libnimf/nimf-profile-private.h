/* Optional benchmark instrumentation, not part of the installed API. */
#ifndef NIMF_PROFILE_PRIVATE_H
#define NIMF_PROFILE_PRIVATE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef NIMF_ENABLE_PROFILING
#include <glib.h>

typedef enum
{
  NIMF_PROFILE_CLIENT_FILTER,
  NIMF_PROFILE_CLIENT_LOCK,
  NIMF_PROFILE_CLIENT_REQUEST,
  NIMF_PROFILE_CLIENT_DISPATCH,
  NIMF_PROFILE_SERVER_FILTER,
  NIMF_PROFILE_ENGINE_FILTER,
  NIMF_PROFILE_SERVER_EMIT,
  NIMF_PROFILE_SEND,
  NIMF_PROFILE_RECEIVE,
  NIMF_PROFILE_WAIT,
  NIMF_PROFILE_HANGUL_PROCESS,
  NIMF_PROFILE_MOZC_KEY,
  NIMF_PROFILE_MOZC_START,
  NIMF_PROFILE_MOZC_REQUEST,
  NIMF_PROFILE_MOZC_READ,
  NIMF_PROFILE_MOZC_PARSE,
  NIMF_PROFILE_CLIENT_STATE,
  NIMF_PROFILE_CLIENT_SIGNAL,
  NIMF_PROFILE_GTK_PREEDIT,
  NIMF_PROFILE_GTK_CURSOR,
  NIMF_PROFILE_N_POINTS
} NimfProfilePoint;

typedef struct
{
  gpointer state;
  guint index;
} NimfProfileScope;

void nimf_profile_begin (NimfProfileScope *scope, NimfProfilePoint point, gint detail);
void nimf_profile_end   (NimfProfileScope *scope);
void nimf_profile_flush (void);

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (NimfProfileScope, nimf_profile_end)

#define NIMF_PROFILE_SCOPE_(point, detail, line) \
  g_auto(NimfProfileScope) G_PASTE (nimf_profile_scope_, line) = { 0 }; \
  nimf_profile_begin (&G_PASTE (nimf_profile_scope_, line), point, detail)
#define NIMF_PROFILE_SCOPE(point, detail) NIMF_PROFILE_SCOPE_ (point, detail, __LINE__)
#else
#define NIMF_PROFILE_SCOPE(point, detail) ((void) 0)
#endif

#endif
