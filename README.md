# Everyone hates reporting time. My pet peeve is TidBank

*Tid* is Norwegian for time. A *tidbank* is the flexitime account at work that you are supposed to fill in every Friday. This TidBank only banks time in a tape loop: it is a tape echo, written and measured by AI agents, with a human who listens and decides.

**[BLACK WEEK OF DEATH: everything 100 % off →](https://stiglau.github.io/tidbank-oss/)**

## What is in here
| | What it is |
|---|---|
| **TidBank** | A tape echo in the style of the Roland RE-201 and Strymon Volante. It has an exact tape-transport model: three playback heads, tempo sync with a nudge, speed inertia that glides the pitch of the repeats like a real motor, and a feedback loop with HPF, LPF and record-head saturation. It also has a BBD (bucket-brigade) engine with profiles kept as data. |
| **Reels** | A reel-only looper locked to the host's bar grid. |
| **Filtær** | A stereo filter with two ZDF ladder filters, modelled on the Moog Matriarch's filter section and checked against one measured unit. |
| **The ears** (`tools/ears/`) | Offline rendering and numeric measurement of timing, glides, aliasing, clicks, stability and spectrum. Agents can't listen, so every claim about the sound is measured here. |
| **The hardware bench** (`tools/matriarch/`, `docs/research/matriarch-*.md`) | How an agent measures a real synth over MIDI and audio with a human at the panel: the rig, the scripts, the traps, and the measured numbers, so you can test your own model against them. |
| **Skills** (`.claude/skills/`) | What an agent needs to know to work on each part: the build, the tests, the model facts and the traps already hit. |

The core in `dsp/` is header-only C++17 with no JUCE dependency and no allocation after `prepare()`. The plugins (VST3/AU/Standalone, arm64 macOS) wrap that core in JUCE 9.

## Jump aboard, agentically
```sh
git clone https://github.com/StigLau/tidbank-oss && cd tidbank-oss
claude          # or your agent of choice; CLAUDE.md and .claude/skills/ tell it how this repo works
```
Then ask it to run `make test`, or to change something and prove the change with the ears.

These also work without an agent:
```sh
make test           # unit tests (ctest), seconds
make ears           # offline render + measurement suite; CASE=s3 for a subset
make s8             # listening set -> build/s8/*.wav
make plugin-check   # JUCE plugin tests without installing (fetches JUCE 9.0.5 once)
make plugin         # builds VST3/AU/Standalone and INSTALLS into ~/Library/Audio/Plug-Ins
```
You need CMake, a C++17 compiler and [uv](https://github.com/astral-sh/uv) (it runs the Python analyzers with numpy).

## How the work runs
1. **Ears before code.** Write the measurement first, watch it fail, then write the DSP. A step is done when its numbers pass.
2. **"Sounds right" is a human verdict.** Agents produce listening sets; a person listens and decides.
3. **Skills hold the knowledge.** When an agent learns a trap, it goes into the matching skill in the same commit.
4. **Claims are checked by someone else.** A second agent with no shared context re-runs the steps before a claim counts.
5. **Delegate by cost.** Small models for skeletons, lookups and re-running checks; big models for hard calls and maths review.

Docs: `docs/whitepaper.md` (architecture, routing, latency, tape/BBD), `docs/design.md` (decisions, parameters), `docs/plan-*.md` (steps with acceptance numbers) and `docs/research/` (curated research and hardware measurements).

## Licence
- **`dsp/`, `tools/`, `tests/`, `docs/` and `site/`:** MIT ([LICENSE-MIT](LICENSE-MIT)). Use the core anywhere.
- **The plugins** (`plugin/`, `reels/`, `filtaer/` and any binary built with JUCE): AGPLv3 ([LICENSE](LICENSE)), because JUCE 9 is AGPLv3 or commercial. If the plugins move off JUCE, this may change.

Third-party code, sources and trademarks are listed in [NOTICE.md](NOTICE.md).
