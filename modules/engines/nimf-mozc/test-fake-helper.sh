#!/bin/sh
# Stands in for mozc_emacs_helper in test-nimf-mozc.
# FAKE_MODE selects the failure to simulate after the session starts.

echo '((mozc-emacs-helper . t)(version . "0")(config . ((preedit-method . roman))))'

while read -r event command rest; do
  event=${event#(}

  case "$command" in
    CreateSession*)
      echo "((emacs-event-id . $event)(emacs-session-id . 7)(output . ()))"
      continue
      ;;
  esac

  case "$FAKE_MODE" in
    exit)    exit 1 ;;
    orphan)  sleep 3 & exit 1 ;;
    hang)    sleep 30 ;;
    garbage) echo 'not an s-expression(' ;;
    error)   echo '((error . session-error)(message . "Session failed"))' ;;
    wrong-event)
      echo "((emacs-event-id . 999)(emacs-session-id . 7)(output . ()))" ;;
    *)
      echo "((emacs-event-id . $event)(emacs-session-id . 7)(output . ((consumed . t)(echo . \"$rest\"))))" ;;
  esac
done
