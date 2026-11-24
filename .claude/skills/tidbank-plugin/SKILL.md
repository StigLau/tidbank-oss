---
name: tidbank-plugin
description: Build, validate and look at the TidBank JUCE plugin (VST3/AU/Standalone, arm64): what is in the UI (sections, engine faces, panic), parameter banks for controllers, pluginval, auval, host-free plugin tests, offscreen UI snapshots. Use when touching plugin/, CMake plugin targets, the editor, or before claiming the plugin works.
---

# TidBank plugin (JUCE 9.0.5 wrapper)

Code: `plugin/PluginProcessor.*` (APVTS params; Engine switch between two mono TapeEngines and two mono BbdEngines per profile, all prepared up front; host tempo; panic), `plugin/PluginEditor.*` (plain JUCE controls in sections by role, status line).

## What the user sees (faces and breaker verified in Ableton Live 2026-11-16, spacing and ping-pong 2026-11-17)
- **One plugin, one window, sections by role**. One fixed size (900 x 504, +24 in probe builds) for both engines; switching Engine only hides and shows sections. THE table of what goes where is `kSectionOf[]` in `plugin/PluginEditor.cpp` (one row per parameter, `static_assert`ed to `NumParams`), plus `kSections[]` (title, face, place on a 6 x 3 cell grid):
  - header: Engine switch;
  - Performance (both): Intensity, Echo;
  - Time and sync (both): one slot showing Division with Sync on and Time with Sync off (two host parameters, only the window swaps them: `TidBankEditor::shown`), Nudge, Sync (decided 2026-11-17);
  - Tape (Tape face): Mode, Inertia, Drive, Spacing (head preset), Target head;
  - Loop filters (Tape face): Loop LPF, Loop HPF;
  - BBD (BBD face, same area as Tape + Loop filters): BBD profile, BBD bright (Dark/Bright, only M-8192 has a Dark set), BBD spacing, Ping-pong, BBD hold droop (Authentic default / Compensated, an A/B of where long delays run away, decided 2026-11-18);
  - Output and safety (both): Dry, Output, Panic.
  - Inside a section, controls follow host order. A new parameter = a row in `kSectionOf` (the build fails without one). A section talks to parameters by id only, never to another section, so it can move to a tab or window later.
  - `plugin_tests` §11: for Tape/BBD x Sync on/off: same window size, a control is visible iff `shown()`, exactly one of Division/Time, nothing overlaps or leaves the window.
  - The ghost-on-the-silent-channel report: `plugin_tests` 10b shows left-only input leaves the right output at 0 (Tape) / -124 dBFS (BBD).
  - Loop LPF/HPF are tape-only on purpose: the question was whether they act on the BBD; they do not (decided 2026-11-16).
- **BBD profiles:** M-8192 (Matriarch), D-4096 (DM-2), C-8192 (Carbon Copy); generic names, no trademarks; profile is not automatable. Feedback is normalised per profile so Intensity = peak loop gain (judged usable on all engines by the listener, 2026-11-16).
- **Nudge** is a detune in cents (+-400) in both modes on both engines: with Sync on it detunes the synced time, with Sync off it scales Time (BBD matched tape 2026-11-17, confirmed by the listener; `plugin_tests` 10c).
- **BBD spacing / Ping-pong** (Matriarch-style): the right line keeps Time (or the synced division); the left line is Time x 2^(spacing/100) free, or the division +-6 steps synced. Ping-pong = mono sum into the left line, L -> R -> L, one Intensity. These three are our choices where the Matriarch manual is silent (`docs/research/delay-stereo-pingpong.md`). Measured since (2026-11-17): the Matriarch's SPACING adds milliseconds to Delay 1 (nearly the same at TIME min and 203 ms), it is not a ratio. The maintainer kept our ratio law (2026-11-18); the left line may run to the profile's `spacedMaxMs` (764 ms on m8192). Tape ping-pong comes later (Volante head pattern, D8).
- **Status line:** host tempo or "No host tempo, fixed 120 BPM" (Standalone), BBD profile, clamp warnings.
- **No output breaker** since 2026-11-17 (the panic button is enough). It was in the plugin earlier (trip over +6 dBFS for 1 s, latched); `dsp/Breaker.h` and its `dsp_tests` stay so it can come back.
- **Mono mic:** no plugin feature; Live's Utility in front, Channel Mode Left (2026-11-16).
- **Coming:** LFO routable to a parameter, its own section.

## MIDI = the host's job (decided 2026-11-16)
No MIDI input, no learn in the plugin: Ableton (or any host) maps controllers to parameters. The **parameter order is the contract** controllers see in banks of 8 (Move/Push): bank 1 Intensity, Division, Mode, Nudge, Inertia, Loop LPF, Loop HPF, Echo; bank 2 Time (sync off), Drive, Spacing, Target head, Sync, Dry, Output, Panic; bank 3 Engine, BBD profile, BBD bright, BBD spacing, Ping-pong, BBD hold droop. There is no Tempo parameter: it only acted without a host tempo, so it was dead in Live; removed 2026-11-17, the Standalone uses 120 BPM. New parameters go at the end unless the maintainer re-decides the banks. Order = `enum Param` = `kIds` = push order in `layout()`; `plugin_tests` §1 asserts every slot. Ids are stable strings, so reordering does not break saved state or automation.

## Commands
```sh
make check JUCE_SRC=<fetched juce-src>   # everything (unit, ears, plugin tests with and without the probe); NEVER installs. Use this.
make plugin        # INSTALLS: fetches JUCE 9.0.5 once, builds VST3/AU/Standalone arm64 into build/plugin, copies to ~/Library/Audio/Plug-Ins
make plugin-test   # INSTALLS, then host-free: bank order, state round trip, dry path bit-exact at echo 0, finite at intensity 1.4 (range raised from 1.2 on 2026-11-18; host automation is stored normalised, so old automation curves land ~14 % lower), panic -> silence, BBD spacing/ping-pong
make pluginval     # strictness 5 on installed VST3 + AU (logs in build/plugin/pluginval-*.log)
make midi-sweep CC=21  # virtual device "TidBank Test" sweeps a CC for 60 s: in Live enable it as Remote input, Cmd-M, click a TidBank param, run
make ui OUT=/tmp/ui.png ARGS="mode=6 intensity=0.9"   # INSTALLS; offscreen editor PNG; choices are 0-based indices
build/plugin-probe/tidbank_snapshot_artefacts/Release/tidbank_snapshot out.png engine=1 [param=v ...]   # no install;
auval -v aufx Tdbk Stlu                               # Apple AU validation (plain effect, no MIDI)
```
Look at the UI with `make ui` and Read the PNG: the agent has no screen-recording permission (`screencapture` fails, `osascript` window queries hang), so do not try to screenshot the Standalone app.

## Debug probe build and never installing
`make plugin` installs into `~/Library/Audio/Plug-Ins` (CMake `TIDBANK_COPY_PLUGIN`, default ON) and so replaces the plugin in use in the DAW. Any experiment, test or probe build uses `-DTIDBANK_COPY_PLUGIN=OFF` in its own build dir: `make plugin-probe` / `make plugin-probe-test` (build/plugin-probe, `JUCE_SRC=<fetched juce dir>` skips the download). Do not run `make plugin`, `make pluginval` or `make midi-sweep`/`make ui` (they depend on `plugin`) from a branch you do not mean to install.
`TIDBANK_DEBUG_PROBE=ON` compiles `plugin/ProbeLog.h` in: off at runtime until env `TIDBANK_PROBE=<seconds>` or `~/Library/Application Support/TidBank/probe.cfg` (`interval=5`); log `~/Library/Logs/TidBank/probe.log` (`TIDBANK_PROBE_LOG`/`TIDBANK_PROBE_CFG` override both paths; the tests use them). Audio thread only pushes POD records into an SPSC ring; a message-thread Timer drains it. `plugin_tests` runs both intervals through the processor (needs `JUCE_MODAL_LOOPS_PERMITTED=1`, set for that target when the probe is on).

## Identity
Manufacturer `Stlu`, plugin `Tdbk`, bundle `com.stiglau.tidbank`, product "TidBank". Never reuse another plugin's codes.

## pluginval
Native arm64 build of the fork `github.com/StigLau/pluginval` (no source changes needed; upstream is Tracktion's pluginval), built in a separate checkout:
```sh
XCODE_VERSION=26 arch -arm64 /opt/homebrew/bin/cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DXCODE_VERSION=26 .
arch -arm64 /opt/homebrew/bin/cmake --build build --config Release -j8
```
The XCODE_VERSION pair is needed because only Command Line Tools are installed: the VST3 SDK's platform check calls `xcodebuild` otherwise. Official releases (v1.0.4, v2.0.0-beta3) are universal binaries too. Risks: deployment target 10.11, CPM fetches deps live, `rtcheck` pinned to a branch.

## Panic
Red PANIC button (UI) and automatable `panic` parameter (map a Move pad/knob to it in Live) (triggers on 0 -> 1): echo fades out in 10 ms, then both engines erase their tape (`reset()`, allocation-free). Dry keeps playing. Asserted in `make plugin-test` (runaway at 1.2 -> exact silence after the fade).

## Traps
- macOS clang pulls `<cstdint>` in transitively; the Linux CI gcc does not: include it explicitly (and `<algorithm>`, `<utility>`) in every file that uses `uint32_t`, `std::max`, `std::make_pair` (Filtær tests broke CI this way).
- A virtual CoreMIDI source is only visible to the SAME process after the run loop runs (`runDispatchLoopUntil` + `JUCE_MODAL_LOOPS_PERMITTED=1`); other apps see it at once.
- Ableton Live: map with Live's MIDI Map (Cmd-M) or let Move/Push follow the device banks; with more than a few params, "Configure" picks which ones Live shows.
- JUCE's CMake needs `C` in `project(... LANGUAGES C CXX)`.
- VST3 validator (inside pluginval) fails if program 0 has no name: `getProgramName` returns "Default".
- `reset()` can run on the audio thread: engines clear in place, never re-prepare.
- Parameter text with 0 decimals: `juce::String(v, 0)` means FULL precision (showed 1.49012e-06 for 0); `fmt(0)` rounds instead.
- Mono in -> stereo out: both engines get the mono input; channel 1 of the buffer is not input then.
- BBD chip noise is not exactly 0: compare "silent" BBD output with < 1e-5, not == 0.
- Installing a test build for someone else: move the old bundles out of `~/Library/Audio/Plug-Ins/{VST3,Components}` into a temporary folder first, `cp -R` from `build/plugin-probe/TidBank_artefacts/Release`, the DAW is restarted; restore with `make plugin` from main.
- **Linux/CI builds** (public repo `build.yml`): JUCE needs `libxi-dev` (XInput2.h) besides the usual X11 packages, and `--no-install-recommends` drops `make`. Release builds set `CMAKE_OSX_DEPLOYMENT_TARGET=11.0`, or the plugin only loads on the runner's macOS. auval needs the AU in `~/Library/Audio/Plug-Ins/Components`, so `tools/release/validate.sh` refuses outside CI.
