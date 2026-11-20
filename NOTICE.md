# NOTICE: what TidBank builds on

TidBank's own code is MIT (`dsp/`, `tools/`, `tests/`, `docs/`, `site/`) or AGPLv3 (the JUCE plugin targets). See [README](README.md#licence). This file credits what the published code and docs build on. Add to it in the same commit as the code or doc that uses something new.

## Third-party software
| What | Used for | Licence | In this repo? |
|---|---|---|---|
| [JUCE 9.0.5](https://github.com/juce-framework/JUCE) | The plugin wrappers (`plugin/`, `reels/`, `filtaer/`) | AGPLv3, or the commercial JUCE 9 licence. TidBank uses AGPLv3 | No. CMake fetches it at build time |
| Steinberg VST3 SDK (bundled with JUCE) | VST3 format | MIT | No. Comes with JUCE |
| [pluginval](https://github.com/Tracktion/pluginval) (Tracktion) | Plugin validation (`make pluginval`) | GPLv3 | No. An external tool, not linked |
| [NumPy](https://numpy.org) | The ears analyzers | BSD-3-Clause | No. Installed by `uv` |
| [SciPy](https://scipy.org) | Bench analysis (`tools/matriarch/`) | BSD-3-Clause | No |
| [mido](https://github.com/mido/mido), [python-rtmidi](https://github.com/SpotlightKid/python-rtmidi) | MIDI on the hardware bench | MIT | No |
| [uv](https://github.com/astral-sh/uv) | Runs the Python tools | MIT or Apache-2.0 | No |

No third-party source code is vendored or copied into this repo. The DSP in `dsp/` is written from the published descriptions listed below. Each header names the paper it follows.

## Papers and books the DSP follows
- V. Zavalishin, *The Art of VA Filter Design*, rev. 2.1.2. The TPT/ZDF state-variable and ladder filters. https://www.discodsp.net/VAFilterDesign_2.1.2.pdf
- C. Raffel and J. Smith, "Practical Modeling of Bucket-Brigade Device Circuits", DAFx-10, 2010. The BBD filters and the compander. https://colinraffel.com/publications/dafx2010practical.pdf
- M. Holters and J. Parker, "A Combined Model for a Bucket Brigade Device and its Input and Output Filters", DAFx-18, 2018. The BBD output hold compensation. https://dafx.de/paper-archive/2018/papers/DAFx2018_paper_12.pdf
- A. Huovilainen, DAFx-05. The compander envelope (eqs. 26–27).
- J. Parker, V. Zavalishin and E. Le Bivic, "Reducing the Aliasing of Nonlinear Waveshaping Using Continuous-Time Convolution", DAFx-16, and the antiderivative anti-aliasing (ADAA) papers by Holters (DAFx-19), Albertini et al. (DAFx-20) and La Pastina and D'Angelo (DAFx20in22). The record-head saturator.
- S. D'Angelo and V. Välimäki, "Generalized Moog Ladder Filter: Part I", IEEE/ACM TASLP 22(12), 2014. https://dangelo.audio/taslp-ladder-part1
- T. E. Stinchcombe, "A Study of the Korg MS10 & MS20 Filters", 2006. https://www.timstinchcombe.co.uk/synth/MS20_study.pdf
- H. Oyama, "Quantifying Nonlinear Behavior in Digital Moog Ladder Filters", DAFx26, 2026. https://dafx26.mit.edu/assets/papers/DAFx26_paper_29.pdf
- J. Chowdhury, "Real-time Physical Modelling for Analog Tape Machines", DAFx-19. https://ccrma.stanford.edu/~jatin/420/tape/TapeModel_DAFx.pdf
- F. Gabrielli, S. D'Angelo and S. Squartini, "Antialiasing in BBD Chips Using BLEP", DAFx-25. https://dafx.de/paper-archive/2025/DAFx25_paper_29.pdf

## Real-time audio practice (`docs/rt-audio-checklist.md`)
These are cited, not copied:
- R. Bencina, "Real-time audio programming 101". http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
- D. Rowland and F. Renn-Giles, "Real-time 101", ADC'19. https://github.com/drowaudio/presentations
- The JUCE documentation and forum threads on real-time programming, APVTS, SmoothedValue and AbstractFifo.
- LWN on priority inversion. https://lwn.net/Articles/452630/

## Code consulted for comparison (none copied)
The research docs compare TidBank's models against these and record their licences there:
- jatinchowdhury18/BBDDelay (BSD-3-Clause)
- Chowdhury-DSP/chowdsp_utils (GPLv3)
- jpcima/bbd-delay-experimental, and the jpcima BBD gist (MIT header)
- Colin Raffel's `echo.cpp`
- ddiakopoulos/MoogLadders (licences vary per model)
- the Faust libraries
- Soundpipe
- MAME `cem3320.cpp`
- Chowdhury-DSP/chowdsp_wdf
- pichenettes/eurorack
- the Surge sst-filters
- Odin 2

## Hardware references and datasheets
- **Moog Matriarch user manual** (Jan 2023): https://cdn.inmusicbrands.com/Moog/Matriarch/Matriarch_Manual_012023.pdf. The manual is cited by page and not redistributed. The measured numbers in `docs/research/matriarch-*.md` come from one unit on our own bench.
- **The Matriarch web editor** by Mark Reid: https://mark.reid.name/matriarch-editor/
- **Roland RE-201 Space Echo** and **Boss RE-20** manuals; **Strymon Volante** manual; **Moog 500 Series Analog Delay**, **MF-104M**, **MF-104S** and **MF-105M** manuals.
- **Datasheets:** Panasonic MN3005/MN3007/MN3008/MN3205/MN3207/MN3208; ON Semiconductor NE570 and AND8159; Signetics SA571; Philips AN174.

Full URLs and page numbers are in `docs/research/bbd-research.md`, `docs/research/bbd-matriarch.md`, `docs/research/delay-stereo-pingpong.md` and `docs/research/filter-matriarch.md`.

## Research runs
Some research was first gathered by AI research runs (Gemini Deep Research, Claude). The raw outputs are not published. The curated docs keep only claims that were checked against the primary sources cited, and they say which claims stay unverified.

## Trademarks
Roland, RE-201 and Space Echo; Boss; Strymon and Volante; Moog and Matriarch; Korg and MS-20; Tascam; Ableton, Live and Move; Steinberg and VST; Apple and Audio Units; JUCE. All are trademarks of their respective owners. They are named only to describe what TidBank models, measures against or runs in. TidBank is not affiliated with or endorsed by any of them.
