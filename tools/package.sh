#!/usr/bin/env bash
# Assembles the Xnounce install zip, its SHA-256 and the release notes (docs/ARCHITECTURE.md § 6).
#
#   tools/package.sh <in-dir> <out-dir> [tag]
#
# <in-dir> holds lin_x64/Xnounce.xpl, win_x64/Xnounce.xpl and mac_x64/Xnounce.xpl (the layout of the CI
# build artifacts). With a tag (vX.Y.Z) it checks the tag against native/CMakeLists.txt and requires a CHANGELOG.md
# section for that version; without one it makes a dev package. Output in <out-dir>:
#   stage/                         Xnounce/ and AirlinePacks/, ready to copy into Resources/plugins/
#   Xnounce-<ver>.zip(.sha256)  the download
#   release-notes.md               the GitHub release body
set -euo pipefail

die() { echo "package.sh: $*" >&2; exit 1; }

[ $# -ge 2 ] && [ $# -le 3 ] || die "usage: tools/package.sh <in-dir> <out-dir> [tag]"
in_dir=$1
out_dir=$2
tag=${3:-}
root=$(cd "$(dirname "$0")/.." && pwd)

version=$(sed -n 's/^project(Xnounce VERSION \([0-9][0-9.]*\).*/\1/p' "$root/native/CMakeLists.txt")
[ -n "$version" ] || die "no project(Xnounce VERSION ...) in native/CMakeLists.txt"

# The CHANGELOG section for this version: everything after "## <version>" up to the next "## " heading,
# without leading blank lines.
changes=$(awk -v v="$version" '
    /^## / { if (found) exit; found = ($2 == v); next }
    found' "$root/CHANGELOG.md" | sed -e '/./,$!d')   # $(...) drops the trailing blank lines

if [ -n "$tag" ]; then
    [ "$tag" = "v$version" ] || die "tag $tag doesn't match native/CMakeLists.txt version $version (expected v$version)"
    [ -n "$changes" ] || die "CHANGELOG.md has no '## $version' section"
    name="Xnounce-$version"
else
    sha=$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo local)
    name="Xnounce-$version-dev.$sha"
    [ -n "$changes" ] || changes="Development build of $version (not a release)."
fi

rm -rf "$out_dir"
stage="$out_dir/stage"
for p in lin_x64 win_x64 mac_x64; do
    [ -s "$in_dir/$p/Xnounce.xpl" ] || die "missing $in_dir/$p/Xnounce.xpl"
    mkdir -p "$stage/Xnounce/$p"
    cp "$in_dir/$p/Xnounce.xpl" "$stage/Xnounce/$p/"
done
cp "$root/README.md" "$root/LICENSE" "$stage/Xnounce/"
# The B612 fonts are compiled in; the SIL OFL requires their licence to travel with them.
mkdir -p "$stage/Xnounce/licenses"
cp "$root/native/fonts/OFL.txt" "$stage/Xnounce/licenses/B612-OFL.txt"

mkdir -p "$stage/AirlinePacks"
cat > "$stage/AirlinePacks/PUT_YOUR_PACKS_HERE.txt" <<'EOF'
Xnounce reads Airline Packs (Fenix A320 format) from this folder.

Put each pack in its own folder, named with the airline's ICAO code:
  AirlinePacks/DLH/
  AirlinePacks/BAW/
  AirlinePacks/Default/      (optional: used when no airline pack matches)

Optional files in this folder:
  liveries.ini   maps livery folder names to airlines (see the Xnounce README)
  airlines.tsv   extra airline codes, read before the built-in table

Packs are third-party content and are not included with Xnounce.
If you add packs while X-Plane is running, use the xnounce/rescan command.
You can delete this file.
EOF

(cd "$stage" && zip -qrX "../$name.zip" Xnounce AirlinePacks)
(cd "$out_dir" && sha256sum "$name.zip" > "$name.zip.sha256")
hash=$(cut -d' ' -f1 "$out_dir/$name.zip.sha256")

repo=${GITHUB_REPOSITORY:-$(git -C "$root" remote get-url origin 2>/dev/null | sed -E 's#^.*github\.com[:/]##; s#\.git$##')}
ref=${tag:-$(git -C "$root" rev-parse HEAD 2>/dev/null || echo main)}

cat > "$out_dir/release-notes.md" <<EOF
$changes

## Install
1. Download \`$name.zip\` and extract it.
2. Copy both folders, \`Xnounce\` and \`AirlinePacks\`, into \`X-Plane 12/Resources/plugins/\`. If you already have
   an \`AirlinePacks\` folder, let them merge: nothing of yours is replaced.
3. Put your Airline Packs (Fenix format, not included) into \`AirlinePacks/\`, one folder per airline ICAO code.
4. Start X-Plane and load a ToLiss A319, A320 or A321.

**Updating:** replace the \`Xnounce\` folder. Your settings in \`Output/preferences/Xnounce.ini\` are kept.
Coming from FXAnnounce (0.3.1 or older): delete the old \`FXAnnounce\` folder and copy \`Xnounce\` in. Your settings
and key bindings carry over.

**macOS:** the plugin isn't notarized by Apple. If X-Plane reports that Xnounce can't be opened or verified,
run this once in Terminal (adjust the path to your X-Plane folder), then restart X-Plane:
\`\`\`
xattr -dr com.apple.quarantine "/Applications/X-Plane 12/Resources/plugins/Xnounce"
\`\`\`

**SHA-256** of \`$name.zip\`: \`$hash\`

Xnounce is free software under the GPL-3.0. Source for this build: https://github.com/$repo/tree/$ref
EOF

echo "package.sh: $out_dir/$name.zip ($version${tag:+, tag $tag})"
