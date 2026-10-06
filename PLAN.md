# CHORALE: plan

CHORALE is the base. TEMPO's best parts come in one at a time, each tested on hardware before
the next, so what works keeps working ("plan B").

## Why not build on TEMPO

A TEMPO-based firmware (`chorale-tempo`, a separate local repo) brought CHORALE's harmonizer
into TEMPO in place of its chromatic engine. The voice engine was correct in isolation, but on
hardware the harmonies played an octave high, strum and stacking misbehaved, and the cause could
not be reproduced off the device. Working back from CHORALE avoids transplanting features that
already work. Kept from that work: TEMPO's take tidying (trim, fades, level), how TEMPO's code
fits together, and two lessons:

- **SDRAM layout.** TEMPO put its sample memory at a fixed address after its own buffers; anything
  else linked into SDRAM landed on top of it (white noise in recordings). Anything that hand-places
  memory has to start after `_esdram_bss`.
- **Card folders.** On the multi-firmware launcher card each firmware lives in its own folder
  (`f_chdir`), with relative paths; stock TEMPO crashed there for lack of samples.

## Steps

1. **TEMPO's granular delay and reverb** on knob 4. Done: `granularDelay.h`, TEMPO's reverb and
   ducking compressor, and `clockManager.h`, whose ticks are counted in samples from the audio
   callback (CHORALE's libDaisy has no timer to spare). Tempo is TEMPO's default until step 3.
2. **The slice engine** (vocal chops). Done (`SliceEngine.h`, TEMPO's `samplePlayer` in
   `SamplePlayer.h`); needs a hardware check. Source: whatever the looper has recorded, cut into 16
   slices on the white keys. the toggle switch flips the keys
   between harmonies and slices. In slice mode, knobs 1-3 are TEMPO's slice controls.
3. **The sequencer and arpeggiator**, on TEMPO's clock. Done (`Sequencer.h`, after TEMPO's
   ArpeggiatorSequencer): play runs it; the wheel is tempo / rate / latch, Chompi + click tap
   tempo; Chompi + play pattern mode, + wheel rests, + loop the raw loop; slice count on Chompi +
   knob 2 in slice mode. Needs a hardware check. Next: chord progressions in chord mode (a step
   per chord, chord types per step), and saving settings.

## Decided controls

See the README. In short: the switch is harmonies / slices; knob 1 is the stack (interval and
direction; Chompi: how many) or the chord type (Chompi: voicing); Chompi key + upper octave is
the root, + lower white keys the scale, + lowest C# / D# the chord octave, + G# the input, + A#
the effects routing, + F# notes / chord mode; the top C is free.

## Open

- Whether the slice engine should also take TEMPO-style separate recordings.
- Saving settings (CHORALE starts from defaults every time).
