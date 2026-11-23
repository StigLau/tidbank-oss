#!/usr/bin/env bash
# Package the built plugins: package.sh <build dir> <platform> <out dir> <juce sha>  -> <out dir>/TidBank-<platform>.zip
set -euo pipefail
b=$1 platform=$2 out=$3 juce=$4
here="$(cd "$(dirname "$0")" && pwd)"; root="$(cd "$here/../.." && pwd)"
d="$out/TidBank-$platform"
[ ! -e "$d" ] || { echo "$d exists"; exit 1; }
mkdir -p "$d/VST3" "$d/Standalone"
for p in TidBank Reels Filtaer; do
  a="$b/${p}_artefacts/Release"
  cp -R "$a/VST3/$p.vst3" "$d/VST3/"
  case "$platform" in
    macOS-*) mkdir -p "$d/AU"; cp -R "$a/AU/$p.component" "$d/AU/"; cp -R "$a/Standalone/$p.app" "$d/Standalone/" ;;
    *) cp "$a/Standalone/$p" "$d/Standalone/" ;;
  esac
done
cp "$root/LICENSE" "$root/LICENSE-MIT" "$root/NOTICE.md" "$here/INSTALL.txt" "$d/"
{ echo "Built from https://github.com/StigLau/tidbank-oss at commit $(git -C "$root" rev-parse HEAD)"
  echo "with JUCE 9.0.5, https://github.com/juce-framework/JUCE at commit $juce"
  echo "These are the sources the AGPLv3 asks for. LICENSE covers the plugins, LICENSE-MIT the core."; } > "$d/SOURCE.txt"
if [ "${platform%%-*}" = macOS ]; then  # ad-hoc: arm64 needs a signature. No Apple Developer ID and not notarized.
  for x in "$d"/VST3/*.vst3 "$d"/AU/*.component "$d"/Standalone/*.app; do codesign --force --sign - "$x"; codesign --verify --strict "$x"; done
fi
(cd "$out" && zip -qry "TidBank-$platform.zip" "TidBank-$platform")
ls -l "$out"/*.zip
