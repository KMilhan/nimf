#!/bin/bash
set -euo pipefail
recipe=$(cd "$(dirname "$0")" && pwd)
cd "$recipe/../../../../.."
for trial in 1 2 3; do
    for distro in ubuntu arch; do
        for language in ja ko; do
            frameworks=(nimf ibus fcitx5)
            if [[ $language == ko ]]; then frameworks+=(upstream); fi
            for ((offset=0; offset<${#frameworks[@]}; offset++)); do
                framework=${frameworks[$(((trial - 1 + offset) % ${#frameworks[@]}))]}
                image="nimf-frameworks-$distro:probe"
                if [[ $distro == arch && $framework == fcitx5 ]]; then image+=-fcitx; fi
                if [[ $framework == upstream ]]; then image="nimf-frameworks-$distro:upstream"; fi
                result="${COMPARISON_RESULTS:?Set a fresh absolute output directory}/trial$trial-$distro-$framework-$language"
                if [[ -f $result/status && $(cat "$result/status") == 0 ]]; then
                    printf 'KEEP trial%s %s %s %s\n' "$trial" "$distro" "$framework" "$language"
                    continue
                fi
                printf 'START %s %s %s %s %s\n' "$(date -Is)" "$trial" "$distro" "$framework" "$language"
                uptime
                ITERATIONS=50 WARMUP=10 bash "$recipe/run-case.sh" \
                    "$distro" "$framework" "$language" "trial$trial" "$image"
                printf 'PASS %s %s %s %s %s\n' "$(date -Is)" "$trial" "$distro" "$framework" "$language"
            done
        done
    done
done
