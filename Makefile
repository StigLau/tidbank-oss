# TidBank: build, test, and "ears" (offline render + measurement). Uses arm64 Homebrew cmake.
CMAKE ?= /opt/homebrew/bin/cmake
B := build
JUCE_SRC ?=# optional: an already fetched JUCE checkout (read-only), skips the download: make check JUCE_SRC=...
.PHONY: help check build test ears s8 plugin plugin-test plugin-check plugin-probe plugin-probe-test pluginval midi-sweep ui clean
.DEFAULT_GOAL := help
help:  # this list (default). Targets marked INSTALLS copy the plugin into ~/Library/Audio/Plug-Ins
	@printf '\033[1;36mTidBank\033[0m  make <target>\n\n'
	@awk -F':' '/^[a-z][a-z0-9-]*:/ { t=$$1; c=$$0; sub(/^[^#]*#? ?/, "", c); if (index($$0, "#") == 0) c=""; \
	  col = (c ~ /^INSTALLS/) ? "\033[1;33m" : "\033[1;32m"; printf "  %s%-18s\033[0m %s\n", col, t, c }' $(MAKEFILE_LIST)
	@printf '\n  \033[2mmost used: make check (everything, no install) · make test · make ears CASE=p_ · make plugin (INSTALLS)\033[0m\n'
check: test ears plugin-check plugin-probe-test  # everything: unit tests, ears, plugin tests with and without the probe. Never installs
	@printf '\033[1;32mcheck: all passed\033[0m\n'
build:  # unit tests + ears renderer (JUCE-free, seconds)
	@$(CMAKE) -B $(B) -DCMAKE_BUILD_TYPE=Release >/dev/null && $(CMAKE) --build $(B) -j
test: build  # unit tests (ctest)
	@cd $(B) && ctest --output-on-failure
ears: build  # make ears CASE=prefix runs a subset
	@tools/ears/run_ears.sh $(CASE)
s8: build  # S8 listening set -> build/s8/*.wav (S8_IN=loop.wav S8_BPM=97 for a real loop)
	@tools/ears/s8.sh
plugin:  # INSTALLS: JUCE VST3/AU/Standalone (arm64) in build/plugin; copies into ~/Library/Audio/Plug-Ins
	@$(CMAKE) -B $(B)/plugin -DTIDBANK_BUILD_PLUGIN=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 >/dev/null && $(CMAKE) --build $(B)/plugin -j
plugin-check:  # plugin tests on a build in build/plugin-check, probe off, never installs
	@$(CMAKE) -B $(B)/plugin-check -DTIDBANK_BUILD_PLUGIN=ON -DTIDBANK_COPY_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 $(if $(JUCE_SRC),-DFETCHCONTENT_SOURCE_DIR_JUCE=$(JUCE_SRC)) >/dev/null && $(CMAKE) --build $(B)/plugin-check -j
	@$(B)/plugin-check/tidbank_plugin_tests_artefacts/Release/tidbank_plugin_tests
	@$(B)/plugin-check/reels_plugin_tests_artefacts/Release/reels_plugin_tests
	@$(B)/plugin-check/filtaer_plugin_tests_artefacts/Release/filtaer_plugin_tests
plugin-test: plugin  # INSTALLS (via make plugin), then host-free plugin checks: MIDI learn on every param, state round trip, bit-exact dry path
	@$(B)/plugin/tidbank_plugin_tests_artefacts/Release/tidbank_plugin_tests
plugin-probe:  # debug-probe plugin in build/plugin-probe. NEVER installs: TIDBANK_COPY_PLUGIN=OFF
	@$(CMAKE) -B $(B)/plugin-probe -DTIDBANK_BUILD_PLUGIN=ON -DTIDBANK_DEBUG_PROBE=ON -DTIDBANK_COPY_PLUGIN=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 $(if $(JUCE_SRC),-DFETCHCONTENT_SOURCE_DIR_JUCE=$(JUCE_SRC)) >/dev/null && $(CMAKE) --build $(B)/plugin-probe -j
plugin-probe-test: plugin-probe  # plugin checks + the probe log checks (1 s via env, 5 s via config file), without touching ~/Library
	@$(B)/plugin-probe/tidbank_plugin_tests_artefacts/Release/tidbank_plugin_tests
PLUGINVAL ?= $(HOME)/src/pluginval/build/pluginval_artefacts/Release/pluginval.app/Contents/MacOS/pluginval
pluginval: plugin  # INSTALLS (via make plugin), then strictness 5 on the installed VST3 and AU (native arm64 build of StigLau/pluginval)
	@tools/pluginval.sh "$(PLUGINVAL)" $(B)/plugin
MIDISEND = $(B)/plugin/tidbank_midisend_artefacts/Release/tidbank_midisend
midi-sweep: plugin  # INSTALLS (via make plugin); virtual device "TidBank Test" sweeps CC $(CC) (default 21) for 60 s: make midi-sweep CC=74
	@$(MIDISEND) sweep $(or $(CC),21) 60
ui: plugin  # INSTALLS (via make plugin); editor snapshot PNG: make ui OUT=/tmp/ui.png ARGS="mode=6 intensity=0.9" (choices are 0-based)
	@$(B)/plugin/tidbank_snapshot_artefacts/Release/tidbank_snapshot $(or $(OUT),$(B)/ui.png) $(ARGS)
clean:  # refuses; delete build/ yourself
	@echo "refusing to rm; delete build yourself if needed"
