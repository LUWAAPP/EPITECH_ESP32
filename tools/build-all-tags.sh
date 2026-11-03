#!/usr/bin/env bash
# Build every tag in an isolated worktree and report OK/FAIL.
# Usage: tools/build-all-tags.sh [env] [tag ...]
set -uo pipefail

ROOT=$(git rev-parse --show-toplevel)
ENV=${1:-ttgo-t7-v14-mini32}
shift || true
TAGS=${*:-$(git tag --sort=creatordate)}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

status=0
for tag in $TAGS; do
    dir="$WORK/$tag"
    printf '%-24s ' "$tag"
    if ! git -C "$ROOT" worktree add --detach "$dir" "$tag" >/dev/null 2>&1; then
        echo "FAIL (worktree)"
        status=1
        continue
    fi
    if (cd "$dir" && pio run -e "$ENV" >"$WORK/$tag.log" 2>&1); then
        echo OK
    else
        echo "FAIL (see $WORK/$tag.log)"
        status=1
    fi
    git -C "$ROOT" worktree remove --force "$dir" >/dev/null 2>&1
done
exit "$status"
