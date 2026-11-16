#!/bin/bash
# Run the cursor location check inside an already built benchmark image.
set -euo pipefail
cd "$(dirname "$0")/../.."
image=${1:?Image is required}
mkdir -p build
results=$(mktemp -d "$PWD/build/cursor-XXXXXX")
echo "Results: $results"
container=$(docker create --init --network none --cap-drop ALL \
    --security-opt no-new-privileges --shm-size 256m \
    --entrypoint /bin/bash "$image" /tmp/session.sh)
printf "%s\n" "$container" > "$results/container.txt"
docker cp tests/cursor/session.sh "$container:/tmp/session.sh"
docker cp tests/cursor/cursor-move-gtk3.c "$container:/tmp/cursor-move-gtk3.c"
docker cp tests/cursor/cursor-move-gtk4.c "$container:/tmp/cursor-move-gtk4.c"
status=0
timeout --foreground --kill-after=10 "${RUN_TIMEOUT:-300}" \
    docker start --attach "$container" > "$results/runtime.log" 2>&1 || status=$?
if docker cp "$container:/results/." "$results/"; then
    docker rm "$container" > /dev/null
else
    echo "Results unavailable; container retained" >&2
    status=1
fi
cat "$results/runtime.log"
exit "$status"
