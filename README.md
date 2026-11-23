# Everyone hates AI. The problem is most don't really know why...

*TidBank*, Norwegian for Time Bank, is an Agentic driven project for designing, analyzing and developing audio software. Just for fun, as part of the AI enshittification. The project is intended for teaching people about agentic development - showing how to do Agentic better. 
The whole project is yolo-developed, but based off software-engineering practices.

**[BLACK WEEK OF DEATH: everything 9999999999999999999999991 % off →](https://stiglau.github.io/tidbank-oss/)**

## What is in here
| | What it is |
|---|---|
| **TidBank** | A tape echo in the style of the Roland RE-201 and Strymon Volante. It has an exact tape-transport model: three playback heads, tempo sync with a nudge, speed inertia that glides the pitch of the repeats like a real motor, and a feedback loop with HPF, LPF and record-head saturation. It also has a BBD (bucket-brigade) engine with profiles such as Matriarch simulation. |
| **Reels** | A simple looper with tape-style controls and undo-functionality |
| **Filtær** | A stereo filter with two ZDF ladder filters, modelled on the Moog Matriarch's filter section and checked against one measured unit. |
| **The ears** (`tools/ears/`) | Offline rendering and numeric measurement of timing, glides, aliasing, clicks, stability and spectrum. Agents can't listen, so every claim about the sound is measured here. |
| **The hardware bench** (`tools/matriarch/`, `docs/research/matriarch-*.md`) | How an agent measures a real synth over MIDI and audio with a human at the panel: the rig, the scripts, the traps, and the measured numbers, so you can test your own model against them. |
| **Skills** (`.claude/skills/`) | What an agent needs to know to work on each part: the build, the tests, the model facts and the traps already hit. |

The core in `dsp/` is header-only C++17 with no JUCE dependency and no allocation after `prepare()`. The plugins (VST3/AU/Standalone, arm64 macOS) wrap that core in JUCE 9.

## Audit it before you run it
> **This repo contains instructions for AI agents** (`CLAUDE.md`, `.claude/skills/`). Once you open an agent in it, those instructions act with your shell and your files. The same goes for every project and skill you download, so **QC and security-audit it first**, this one included.

The short version. Stay outside the project, and ask an agent that can only read:
```sh
mkdir audit && cd audit && git clone https://github.com/StigLau/tidbank-oss
# put prompt A from the guide in prompt.txt, then use one of these, or better both:
claude -p --safe-mode --restricted --strict-mcp-config --tools "Read,Grep,Glob" --no-session-persistence < prompt.txt
pi -p --no-approve --no-context-files --no-skills --no-extensions --no-prompt-templates --no-mcp --no-session --tools read,grep,find,ls "$(cat prompt.txt)"
```
**[docs/security-audit.md](docs/security-audit.md)** has the prompts (whole project, a single skill, an update, and QC of the claims), a check for hidden characters, and what an audit can't tell you. Read the skills yourself too: an agent's silence is not evidence.

## Jump aboard, agentically
When you are happy with what you found:
```sh
cd tidbank-oss
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

## Contribution and support
I'm open to take in PR contributions and tests/metering of other devices.
Support small devs and musicians! 
I'm not asking for anything. This is my contribution to others.
