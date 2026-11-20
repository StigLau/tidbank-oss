# TidBank

A tape echo (RE-201 / Volante style), a BBD engine, a looper (Reels) and a Matriarch-style filter (Filtær), built agent-first: agents write and verify the DSP, a human listens and decides. Header-only, JUCE-free C++17 core in `dsp/`; offline "ears" in `tools/ears/` that measure every claim; JUCE 9 plugin wrappers in `plugin/`, `reels/` and `filtaer/`.

## How work runs here
1. **Skills hold the knowledge.** Load the matching skill before you work. When you learn a trap, add it to the skill in the same commit.
2. **Ears before code.** Write the measurement or assertion first, watch it fail, then write the DSP. A step is done when its numbers pass. "Sounds right" is only a human's verdict, given on a listening set such as `make s8`.
3. **Claims need evidence.** Report only what you ran, with the numbers. If you can, have a second agent with no shared context re-run the steps before a claim counts.
4. **Ask up front, then work through.** Collect the questions first. Then take each item to either done-with-numbers or blocked, and end with one summary of what waits on the human.
5. **Delegate by cost.** Use small models for skeletons, lookups and re-running checks, and big models for hard calls and for reviewing maths or design. Give agents the questions and the files, not your hypothesis.

| Skill | Use for |
|---|---|
| `tidbank-architecture` | Design, layout, build, order of work, delegation |
| `tidbank-tape-engine` | Changing the tape engine; model facts and traps |
| `tidbank-ears` | Measuring any DSP change (render + analyze, suite cases) |
| `tidbank-rt` | Audio-thread rules, latency |
| `tidbank-plugin` | JUCE plugin: build, pluginval, auval, plugin tests, UI snapshots, parameter banks, panic |
| `reels-plugin` | Reels, the looper: timing model, tests, build |
| `filtaer-plugin` | Filtær, the Matriarch-style filter: model, tests, build, what v0 leaves out |
| `matriarch-bench` | Measuring a Moog Matriarch (rig, scripts, traps, how to validate a hardware number) |

## Commands
```sh
make test               # unit tests (ctest), seconds
make ears               # full measurement suite; CASE=s3 for a subset
make s8                 # listening set -> build/s8/*.wav (S8_IN=loop.wav S8_BPM=97 for a real loop)
make plugin-check       # plugin tests in build/plugin-check, never installs
make plugin             # VST3/AU/Standalone arm64, INSTALLS into ~/Library/Audio/Plug-Ins (fetches JUCE 9.0.5 once)
make pluginval          # strictness 5, VST3 + AU
make ui OUT=/tmp/ui.png # look at the editor (offscreen PNG)
```
`make clean` refuses on purpose. Ask before deleting `build/`.

## Conventions
- **`dsp/` stays JUCE-free and header-only, with no allocation after `prepare()`.** It is MIT; the plugin targets are AGPLv3 (see README).
- **No generated files in the repo root.** Build output goes to `build/` (gitignored).
- **This is a public repo.** Commits, issues and PRs carry no session ids, no agent-run footers, no local paths and no personal or machine details. `Agent-Model: <id>` trailers are welcome.
- **Credit what you build on.** New third-party code, papers or measurements go into `NOTICE.md` in the same commit.
