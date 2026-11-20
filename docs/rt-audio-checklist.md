# Real-time audio checklist

Condensed from a cited research run (2026-11, sources below); applies to `dsp/` and the future `plugin/`. "(holds in dsp/)" marks what was checked in the code.

## Audio-thread rules
- [ ] No heap allocation or free in `processBlock`/`process` (no growing `std::vector`/`std::string`, no `std::function` construction). (holds in dsp/: buffers are sized in `prepare()`)
- [ ] No mutex or lock; a lock held by the GUI thread blocks audio (priority inversion), even with priority inheritance.
- [ ] No file/network I/O, logging, printf or other blocking system calls; stream data via a lock-free ring buffer and an I/O thread.
- [ ] Call only code known to be real-time safe; opaque APIs (Objective-C, COM, driver calls) may allocate, lock or page.
- [ ] No unbounded or amortised work (rehash, lazy init, "fast 99.9 % of the time"); one spike longer than the buffer period glitches.
- [ ] Do allocation, file loading and sinc/table building in `prepare()`, sized for the maximum block size and maximum delay. (holds in dsp/: `DelayLine::prepare`, `TapeTransport::prepare`)
- [ ] `TapeEngine::reset()` is allocation-free (`DelayLine::clear` zeroes in place), so it is safe on the audio thread; only `prepare()` allocates. (holds in dsp/)
- [ ] Never call `AudioProcessorEditor`/UI code from `processBlock`; post to the message thread via `AsyncUpdater`/`ChangeBroadcaster`.
- [ ] If another thread uses `getCallbackLock()`, hold it as briefly as possible; use `suspendProcessing()` for long reconfiguration.
- [ ] Check `std::atomic<T>::is_always_lock_free` for every atomic type shared with audio (`static_assert`).
- [ ] No per-sample branches on parameters in hot loops; branch once per block where possible.

## Parameters and smoothing
- [ ] Smooth every audible parameter, or zipper noise appears. (holds in dsp/: `FeedbackNetwork` smooths HPF, LPF, intensity and drive with a 20 ms one-pole; speed and head distance glide via the transport inertia; head on/off fades over 20 ms)
- [ ] Smooth frequency and dB-like parameters in the log domain (multiplicative); linear for gains and pans. (holds in dsp/: cutoffs smoothed as `log(Hz)`, drive in dB)
- [ ] First `setParams`/`setTargets` after `prepare`/`reset` snaps instead of ramping from zero. (holds in dsp/)
- [ ] Clamp every incoming value (cutoffs, intensity, BPM, time) before use; a host or CC can send anything. (holds in dsp/ for cutoffs via `clampHz`, BPM and time via `max`)
- [ ] Read APVTS values with `getRawParameterValue()` (returns `std::atomic<float>*`); cache the pointers in the constructor, do one atomic load per block, no string lookup in `processBlock`.
- [ ] Never touch the APVTS `ValueTree` from the audio thread; only the atomic parameter values are RT-safe.
- [ ] `parameterChanged` listeners may run on the audio or message thread; the body may only set atomics or flags.
- [ ] Anything that jumps (mode, spacing, head target) cross-fades or glides; do not switch it in one sample.

## Denormals and NaN
- [ ] `juce::ScopedNoDenormals` as the first line of `processBlock`; hosts do not reliably set FTZ/DAZ, and the scoped version restores the host's FP state.
- [ ] Also flush decaying feedback and filter state yourself: denormals appear in IIR filters, delay feedback and envelope followers. (holds in dsp/: `FeedbackNetwork` Svf states flushed below 1e-15, smoothers snap within 1e-9)
- [ ] Keep state in double. (holds in dsp/)
- [ ] Guard singular points with a clamp or Taylor branch (Chowdhury: `x/3` for tiny `|x|` in the Langevin function). (holds in dsp/ for the ADAA saturator: `kAdaaEps` fallback, overflow-safe `logcosh`, output clamped to 1/g)
- [ ] Prefer rational/Padé tanh (clamped input) over libm `tanh` or LUTs if the saturator ever becomes a hot spot; ADAA already cuts aliasing without oversampling.

## Latency
- [ ] Report the real latency to the host; the plugin reports 0 and the dry path must never be delayed. (holds in dsp/: wet-path look-ahead is absorbed in `kReadOffset`)
- [ ] No oversampling or linear-phase filters on the dry path. Polyphase IIR oversampling is not zero-latency either; call `getLatencyInSamples()`.
- [ ] Keep `kReadOffset` far below the shortest head delay (40 ms = 1920 samples at 48 kHz; 1.5 used). (holds in dsp/)

## Threading between audio and message threads
- [ ] Scalars: `std::atomic` with relaxed order is enough for independent values.
- [ ] Events, meter data, MIDI-learn messages: SPSC FIFO (`juce::AbstractFifo` over preallocated storage), exactly one producer and one consumer.
- [ ] Larger objects (coefficient sets): double-buffer and publish with an atomic pointer swap (release store, acquire load); the audio thread must never free the old object, defer deletion to the message thread.
- [ ] Never use `std::atomic_load` on `std::shared_ptr` (it falls back to locks); use `juce::ReferenceCountedObject` or POD FIFOs.
- [ ] Use release/acquire whenever an atomic flag or pointer publishes other non-atomic data.
- [ ] Meters and tempo readouts for the UI: audio writes atomics, a UI `Timer` (about 15-30 Hz) reads them; repaint only when the value changed.

## Plugin wrapper (plugin/)
- [ ] `prepareToPlay`: call `prepare(sampleRate, maxDelaySec)`, size scratch buffers for `samplesPerBlock`, re-run it on every sample-rate change (smoothing coefficients and the delay length depend on it).
- [ ] `processBlock` must accept a block size of 0 (return early) and sizes that vary from call to call and exceed `samplesPerBlock`; the sample count "is NOT guaranteed" constant.
- [ ] Tempo: read host BPM via `AudioPlayHead` once per block; handle a missing playhead or BPM (fall back to the last value or a default) and BPM 0.
- [ ] MIDI CC: map to parameters through the message-thread-safe path (atomic or FIFO); do not allocate while iterating the `MidiBuffer`.
- [ ] Bypass: keep the dry signal at unity latency and cross-fade into and out of bypass to avoid clicks.
- [ ] State save/restore: `getStateInformation`/`setStateInformation` use the APVTS state on the message thread only; accept missing or extra parameters, and call `reset` outside `processBlock`.
- [ ] Run `pluginval` before each release and fix every failure; it exercises state restore, varying block sizes and sample-rate changes.

## Testing
- [ ] `make test && make ears` after every DSP change; report the numbers, not "sounds right".
- [ ] Ears `health` checks for NaN/Inf, subnormals and peak on noise, extreme settings and all 12 modes. (holds in dsp/: `s3_runaway_health`, `s4_mode*_health`, `s7_extreme_health`)
- [ ] Click-free parameter jumps are asserted (`s4_click_*`); add a case for every new jumping parameter.
- [ ] Measure CPU in release builds only (one forum test: debug 40-60 % vs release 6 % for the same load); keep the `s7_cpu` case as a regression guard.
- [ ] Render the same input with different block sizes (including 1 and non-power-of-two) in a host-like harness; output must match.

## Sources
- Ross Bencina, Real-time audio programming 101: http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
- LWN on priority inversion and locks: https://lwn.net/Articles/452630/
- Renn-Giles and Rowland, Real-time 101 (ADC'19) slides: https://github.com/drowaudio/presentations
- JUCE, real-time programming in audio development: https://juce.com/posts/real-time-programming-in-audio-development/
- JUCE `AudioProcessor` docs (block size, callback lock): https://docs.juce.com/master/classAudioProcessor.html
- JUCE `AudioProcessorValueTreeState` docs: https://docs.juce.com/master/classAudioProcessorValueTreeState.html
- JUCE forum, APVTS raw value thread safety: https://forum.juce.com/t/audioprocessorvaluetreestate-raw-value-pointer-thread-safety/29957
- JUCE `SmoothedValue` docs: https://docs.juce.com/master/classSmoothedValue.html
- JUCE `AbstractFifo` docs: https://docs.juce.com/master/classAbstractFifo.html
- JUCE forum, sharing data across threads (Doumler): https://forum.juce.com/t/timur-doumler-talks-on-c-audio-sharing-data-across-threads/26311
- JUCE forum, denormal prevention: https://forum.juce.com/t/state-of-the-art-denormal-prevention/16802
- Chowdhury, tape model (DAFx-19): https://ccrma.stanford.edu/~jatin/420/tape/TapeModel_DAFx.pdf
