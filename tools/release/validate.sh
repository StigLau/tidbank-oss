#!/usr/bin/env bash
# Validate an unpacked package in CI: validate.sh <package dir> <dir with pluginval>. On macOS it copies the AUs into
# ~/Library/Audio/Plug-Ins/Components (Audio Units must be there to load), so it refuses to run outside CI.
set -uo pipefail
[ "${CI:-}" = true ] || { echo "CI only: this installs Audio Units"; exit 2; }
d=$1 pv=$2 rc=0
if [ "$(uname)" = Darwin ]; then run=("$pv/pluginval.app/Contents/MacOS/pluginval"); else chmod +x "$pv/pluginval"; run=(xvfb-run -a "$pv/pluginval"); fi
check() { echo "::group::$*"; "$@"; local r=$?; echo "::endgroup::"; if [ $r = 0 ]; then echo "PASS $*"; else echo "FAIL ($r) $*"; rc=1; fi; }
for p in "$d"/VST3/*.vst3; do check "${run[@]}" --strictness-level 5 --validate "$PWD/$p"; done
if [ "$(uname)" = Darwin ]; then
  c="$HOME/Library/Audio/Plug-Ins/Components"; mkdir -p "$c"; cp -R "$d"/AU/*.component "$c/"
  killall -9 AudioComponentRegistrar 2>/dev/null; sleep 5
  for code in Tdbk Rels Fltr; do check auval -v aufx $code Stlu; done
fi
exit $rc
