#!/bin/sh
# Supply-chain pins (make pins): whatever the build, the ears and CI download is named by commit or locked by hash.
# Not covered: tools/matriarch (run by hand at the bench), Homebrew/apt packages, the Python interpreter.
# Optional arg = root to check, so the check itself can be tested on a broken copy.
cd "${1:-$(dirname "$0")/..}" || exit 2
bad=0
fail() { printf 'PIN  %s\n%s\n' "$1" "$2"; bad=1; }

x=$(grep -rnE '^[[:space:]]*(-[[:space:]]+)?uses:' .github/workflows | grep -vE 'uses:[[:space:]]+(\./|[^@[:space:]]+@[0-9a-f]{40}([[:space:]]|$))')
[ -z "$x" ] || fail 'GitHub action not pinned by full commit' "$x"

x=$(grep -rnE '^[[:space:]]*GIT_TAG' CMakeLists.txt cmake | grep -vE 'GIT_TAG[[:space:]]+[0-9a-f]{40}([[:space:]]|$)')
[ -z "$x" ] || fail 'CMake GIT_TAG is not a full commit' "$x"

# build.yml reads the JUCE commit from here: exactly one
n=$(grep -cE '^[[:space:]]*GIT_TAG[[:space:]]' CMakeLists.txt)
[ "$n" = 1 ] || fail "expected one GIT_TAG in CMakeLists.txt, found $n" "$(grep -n GIT_TAG CMakeLists.txt)"

x=$(grep -rnE '(^|[^[:alnum:]_])(uv run|uvx|uv tool|pip3? install)([[:space:]]|$)' tools/ears Makefile .github/workflows --include='*.sh' --include='*.py' --include='*.yml' --include=Makefile | sed -E 's/uv run (--quiet )?--locked --script [^ ]+\.py//g' | grep -E '(uv run|uvx|uv tool|pip3? install)([[:space:]]|$)')
[ -z "$x" ] || fail 'Python not run from a lock (uv run --locked --script x.py; --with and --with-requirements do not check hashes)' "$x"

for f in tools/ears/*.py; do
  grep -qE '^# /// script' "$f" || continue
  [ -f "$f.lock" ] || fail 'script header without a committed lock (uv lock --script)' "$f"
done

# one numpy for everything, so a number does not depend on which script measured it
n=$(cat tools/ears/*.py.lock 2>/dev/null | grep -A1 '^name = "numpy"' | grep '^version' | sort -u | wc -l | tr -d ' ')
[ "$n" = 1 ] || fail "expected one numpy version across tools/ears/*.lock, found $n" "$(grep -A1 '^name = "numpy"' tools/ears/*.py.lock 2>/dev/null | grep version)"

[ $bad = 0 ] && echo 'pins: ok'
exit $bad
