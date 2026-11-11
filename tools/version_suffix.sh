#!/usr/bin/env bash
# Prints the version suffix for the build: "-rc.N" when CI builds a release-candidate tag (vX.Y.Z-rc.N),
# otherwise nothing. CMake appends it to the version the plugin reports (XN_VERSION_SUFFIX).
set -euo pipefail
ref=${GITHUB_REF:-}
case "$ref" in
    refs/tags/v*-rc.*) printf -- '-rc.%s' "${ref##*-rc.}" ;;
esac
