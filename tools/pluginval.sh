#!/bin/sh
# pluginval strictness 5 on the installed TidBank VST3 and AU. Usage: tools/pluginval.sh <pluginval binary> <log dir>
# The binary is a native arm64 build of github.com/StigLau/pluginval (see .claude/skills/tidbank-plugin).
PV=$1; LOG=$2; rc=0
[ -x "$PV" ] || { echo "pluginval not found at $PV (set PLUGINVAL=...)"; exit 2; }
for p in VST3/TidBank.vst3 Components/TidBank.component; do
    log="$LOG/pluginval-$(basename "$p").log"
    printf "%-28s" "$p"
    if "$PV" --strictness-level 5 --validate "$HOME/Library/Audio/Plug-Ins/$p" > "$log" 2>&1; then echo PASS; else echo "FAIL (see $log)"; rc=1; fi
done
exit $rc
