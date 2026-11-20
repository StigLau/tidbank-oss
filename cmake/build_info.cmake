# Writes BuildInfo.h with the commit the plugin was built from, so the editors' status lines name the build (2026-11-18).
# Run on every build by the tidbank_build_info target; configure_file rewrites the header only when the text changes.
# "-dirty" = uncommitted changes to tracked files; "unknown" = not built from a git checkout.
execute_process(COMMAND git rev-parse --short HEAD WORKING_DIRECTORY "${SRC}" OUTPUT_VARIABLE hash
                OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE rc ERROR_QUIET)
if(NOT rc EQUAL 0)
  set(hash "unknown")
else()
  execute_process(COMMAND git diff --quiet HEAD WORKING_DIRECTORY "${SRC}" RESULT_VARIABLE dirty)
  if(NOT dirty EQUAL 0)
    string(APPEND hash "-dirty")
  endif()
endif()
file(WRITE "${OUT}.tmp" "#pragma once\n#define TIDBANK_BUILD \"${hash}\"\n")
configure_file("${OUT}.tmp" "${OUT}" COPYONLY)
