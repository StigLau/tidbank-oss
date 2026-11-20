# Reflections: routing and "faces"

Thoughts on the subject, kept in case routing has to be thought about differently later. Nothing here is decided. The alternatives and experiments were discussed in the project's issue tracker.

## The maintainer, 2026-11-16 (notes, lightly edited)

- "Chained in the DAW, never linked" was the first proposal for how a separate tape-ageing plugin and TidBank fit together, not a hard requirement. The idea was that it is simpler to let the user wire the plugins together with the DAW's normal UI.
- Then the problem of having several such plugins came up: Reels (the looper), the tape-ageing plugin, TidBank Tape and TidBank BBD.
- If we end up with several "faces" on top of one base architecture, what do the faces do? The assumption:
  - they talk to the user and carry configuration parameters (MIDI integration and the like);
  - they don't need to carry audio at runtime;
  - one of these plugins carries the main logic.
- Open worry: this probably contradicts how VSTs work today.
- Ableton Live is accepted as the runtime environment for now. Ableton Move is not a requirement.

## Agent reasoning, 2026-11-16: does the faces idea fit how plugins work?

Reasoning only, nothing measured yet.

- **Audio can't be split across faces.** A host gives each plugin instance only the audio of the track it sits on, and runs unrelated tracks in parallel in no fixed order. So all audio has to be processed in one place: the "core" instance on the track that carries the sound. That part of the idea holds.
- **A face that carries only parameters is possible.** It is a plugin with its own parameters and editor, which passes them to the core through shared memory in the same process. Live loads plugins in one process; Bitwig sandboxes them.
  - Parameters are not timing-critical. A one-block lag between a face and the core is inaudible for a knob, unlike for audio.
  - So the cross-track jitter that rules out sharing audio (the alternative of sharing the reel between tracks) doesn't rule out sharing configuration.
- **What a faces design must solve:**
  - **Pairing.** Which face controls which core after a Live set is reloaded, when there are two cores. That needs a pairing id saved in both states.
  - **State ownership.** Live saves each plugin's state separately. Either the core owns all settings and a face only mirrors them, or each face saves its own settings and the core pulls them in.
  - **Automation.** Live records automation on the face's parameters. The core must accept changes from a face without echoing them back.
  - **A face with no core.** It must say so plainly and never silently do nothing, since a silent fallback counts as a failure.
- **Precedents:**
  - Kilohearts Snap Heap and Soundtoys Effect Rack keep the audio and the modules in one host plugin.
  - Blue Cat Connector shares audio between instances, in the same process.
  - None of these is a parameter-only face controlling a core, as far as we found. That would be our own design.
- **A review model's recommendation is close to this.** The "print head" is a second plugin that shares the reel in-process with Reels, and a shipped Live rack's macros act as the face. If small separate plugins wired by Live turn out too limited, parameter-only faces on one core is the next step, and it doesn't contradict how VSTs work as long as all audio stays in the core.
