#include <nimf.h>
#include <stdio.h>
#include <string.h>

static guint commits;

static void
on_commit (NimfIM *im, const char *text, gpointer data)
{
  (void) im;
  (void) data;
  g_assert_cmpstr (text, ==, "日本語");
  commits++;
}

static void
key (NimfIM *im, guint keyval)
{
  NimfEvent event = { .key = { .type = NIMF_EVENT_KEY_PRESS,
                              .keyval = keyval } };
  nimf_im_filter_event (im, &event);
}

static void
compose (NimfIM *im)
{
  for (const char *p = "nihongo"; *p; p++)
    key (im, *p);
  key (im, NIMF_KEY_space);
}

int
main (void)
{
  setvbuf (stdout, NULL, _IOLBF, 0);
  NimfIM *im = nimf_im_new ();
  g_signal_connect (im, "commit", G_CALLBACK (on_commit), NULL);
  nimf_im_focus_in (im);
  puts ("ready");
  char command[64];
  while (fgets (command, sizeof command, stdin))
  {
    guint target;
    if (strcmp (command, "compose\n") == 0)
    {
      compose (im);
      puts ("candidate");
    }
    else if (sscanf (command, "until %u", &target) == 1)
    {
      if (commits == 0)
        key (im, NIMF_KEY_Return);
      while (commits < target)
      {
        guint before = commits;
        compose (im);
        key (im, NIMF_KEY_Return);
        g_assert_cmpuint (commits, ==, before + 1);
      }
      printf ("commits %u\n", commits);
    }
    else if (strcmp (command, "quit\n") == 0)
      break;
    else
      g_error ("Unknown command: %s", command);
  }
  nimf_im_focus_out (im);
  g_object_unref (im);
  printf ("closed %u\n", commits);
  return 0;
}
