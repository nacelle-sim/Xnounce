#!/usr/bin/env bash
# Formats the C++ sources with clang-format, or checks them.
#
#   tools/format.sh           rewrite the files in place
#   tools/format.sh --check   exit 1 if any file would change (what CI runs)
#
# CI pins clang-format 23 (.github/workflows/build.yml). Another major version
# may format differently, so use 23 locally as well.
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

# Our own sources. fonts/*.inc are generated arrays and are left alone.
mapfile -t files < <(git ls-files -co --exclude-standard \
    'src/*.cpp' 'src/*.h' 'tests/*.cpp' 'tests/*.h' 'tools/*.cpp' 'tools/*.h')
[ ${#files[@]} -gt 0 ] || { echo "format.sh: no sources found" >&2; exit 1; }

if [ "${1:-}" = "--check" ]; then
    clang-format --dry-run --Werror "${files[@]}"
    echo "format.sh: ${#files[@]} files are formatted"
else
    clang-format -i "${files[@]}"
    echo "format.sh: formatted ${#files[@]} files"
fi
