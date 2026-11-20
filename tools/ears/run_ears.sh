#!/bin/sh
# Thin wrapper: all cases live in suite.py. Optional arg = case-name prefix. Scratch under ${TMPDIR}/tidbank-ears.
cd "$(dirname "$0")/../.." || exit 2
exec uv run --quiet --with numpy tools/ears/suite.py "$@"
