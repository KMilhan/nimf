#!/bin/bash
# Configure only the private profile created by session.sh.
# The sourcing caller uses the selected framework, language, engine, and server PID.
# shellcheck disable=SC2034
set -euo pipefail
result=${1:?Result directory is required}
framework=${IM_FRAMEWORK:-nimf}
language=${INPUT_LANGUAGE:-ja}
case "$language" in ja|ko) ;; *) echo 'INPUT_LANGUAGE must be ja or ko' >&2; exit 2 ;; esac
case "$framework" in
    nimf)
        export GTK_IM_MODULE=nimf
        engine=mozc
        if [[ $language == ko ]]; then engine=libhangul; fi
        gsettings set org.nimf setup-environment-variables false
        for candidate in rime anthy libhangul mozc; do
            active=false
            if [[ $candidate == "$engine" ]]; then active=true; fi
            gsettings set "org.nimf.engines.nimf-$candidate" active-engine "$active"
        done
        gsettings set org.nimf.engines default-engine "nimf-$engine"
        nimf --no-daemon > "$result/nimf.log" 2>&1 &
        server=$!
        ;;
    ibus)
        export GTK_IM_MODULE=ibus
        engine=mozc-jp
        if [[ $language == ko ]]; then
            engine=hangul
            gsettings set org.freedesktop.ibus.engine.hangul initial-input-mode hangul
            gsettings set org.freedesktop.ibus.engine.hangul hangul-keyboard 2
            gsettings set org.freedesktop.ibus.engine.hangul auto-reorder false
            gsettings set org.freedesktop.ibus.engine.hangul word-commit false
        else
            mkdir -p "$XDG_CONFIG_HOME/mozc"
            cat > "$XDG_CONFIG_HOME/mozc/ibus_config.textproto" <<'EOF'
engines {
  name: "mozc-jp"
  longname: "Mozc"
  layout: "us"
  rank: 80
}
active_on_launch: True
EOF
        fi
        gsettings set org.freedesktop.ibus.general preload-engines "['$engine']"
        ibus-daemon --emoji-extension disable --cache refresh \
            > "$result/ibus.log" 2>&1 &
        server=$!
        ;;
    fcitx5)
        export GTK_IM_MODULE=fcitx
        engine=mozc
        if [[ $language == ko ]]; then engine=hangul; fi
        mkdir -p "$XDG_CONFIG_HOME/fcitx5/conf"
        cat > "$XDG_CONFIG_HOME/fcitx5/profile" <<EOF
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=$engine

[Groups/0/Items/0]
Name=keyboard-us
Layout=

[Groups/0/Items/1]
Name=$engine
Layout=

[GroupOrder]
0=Default
EOF
        cat > "$XDG_CONFIG_HOME/fcitx5/config" <<'EOF'
[Behavior]
ActiveByDefault=True
ShareInputState=All
EOF
        cat > "$XDG_CONFIG_HOME/fcitx5/conf/hangul.conf" <<'EOF'
Keyboard=Dubeolsik
AutoReorder=False
WordCommit=False
HanjaMode=False
EOF
        fcitx5 -D --disable=wayland > "$result/fcitx5.log" 2>&1 &
        server=$!
        ;;
    *) echo 'IM_FRAMEWORK must be nimf, ibus, or fcitx5' >&2; exit 2 ;;
esac
