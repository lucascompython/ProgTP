#!/usr/bin/env bash
set -o pipefail
build_dir="$1"
shift
exit_status=0
for f in "$@"; do
    clang-tidy -p "$build_dir" "$f" 2>&1 | grep -v "/subprojects/"
    status="${PIPESTATUS[0]}"
    if [ "$status" -ne 0 ]; then
        exit_status="$status"
    fi
done
exit "$exit_status"
