# CHORALE

A live harmonizer for CHOMPI.

> **Unofficial community firmware.** CHORALE is a modified version of SING (by
> [ugrossek](https://github.com/ugrossek/CHOMPI)), which is built on CHOMPI's TAPE firmware. It is
> not an official CHOMPI Club or Chase Bliss release; see [`TRADEMARKS.md`](TRADEMARKS.md).

Sing into the built-in mic (or plug a mic or line source into the input jack) and hold keys:
every held key adds a copy of your voice, pitch-shifted by that key's distance from the
**middle C**. Hold C and E and you hear a third; C, E and G give a major chord. Whatever note
you sing counts as the middle C. The 25 keys cover one octave down to one octave up, up to 7
voices at a time. The looper and effects work as in TAPE, on your voice and the harmonies.

**An external mic on the input jack is strongly recommended.** The built-in mic sits on the
same board as the keys and picks up their clicks.

## Controls

Each knob has pages (press it to switch). Holding the Chompi key gives each page a second
function: hold the Chompi key and turn.

| Knob | Page 1 | Chompi key + turn | Page 2 | Chompi key + turn |
|---|---|---|---|---|
| 1 | notes mode: **stack**, adds notes to the held chord, one per click · chord mode: **chord type** | **scale**: major, minor, dorian, phrygian, lydian, mixolydian, harmonic minor, major/minor pentatonic | **strum**: centre all at once; right upward, left downward; further out, longer gap (up to 500 ms) | **glide** |
| 2 | harmony **attack**, 2 ms…500 ms | **input threshold**, −70…−20 dB (default −50; fully left: off) | filter envelope **attack**, 1 ms…2 s | filter **cutoff**, 40 Hz…16 kHz |
| 3 | harmony **release**, 20 ms…3 s; fully right: never fade | **freeze threshold**, −60…−5 dB | filter envelope **decay**, 10 ms…5 s | filter **resonance** |
| 4 | **reverb/delay** · **bitcrush** · **doubler** (three pages) | delay time · output compression · warble | | |
| 5 | looper, as in TAPE | | | |
| 6 | **output volume** | **spread**: voices alternately left and right | **dry/wet**: left only your voice, centre both (default), right only the harmonies | **input gain** |

**Chompi key + press:**

| Knob | |
|---|---|
| 1 | next stack interval: 3rds, 4ths, 5ths, 6ths, octaves (page 2: strum back to the centre) |
| any key | sets the **root** (tonic) of the scale to that note |
| 2 | filter envelope on/off (starts off) |
| 3 | freeze on/off (starts off) |
| 4 | reset the effects: reverb, bitcrush, doubler, delay time, compression, warble |
| 6 | where your unshifted voice goes: headphones (orange), all outputs (blue) or off (dim red) |

**Hold knob 6** for an input meter across the keys (green, yellow from −12 dB, red from −4 dB).

**Toggle switch: notes or chords.** In one position each key plays one note (**notes mode**);
in the other (the one that turned TAPE's mic monitor off) each key plays a whole chord
(**chord mode**). Either way, notes fade with the release when you let go.

Every start begins from the defaults; CHORALE does not save knob settings.

## Scale and root

Hold the Chompi key and turn knob 1 to pick a scale, and press a key to set its root; the keys
show the scale while you hold it (the root bright amber, the other scale notes dim). The root is
counted from your voice, which is always the middle C: with root C, your voice is the root; with
root G, your voice is the 4th of G. The stack and the chords both follow the scale, so each
chord's quality comes out of it. A note outside the scale counts as the scale note below it.

## Chord mode

Each key plays a chord built on it from the scale, one chord at a time: the newest held key's,
and letting go of it goes back to the key held before. In C major, C plays C major, E plays E
minor, B plays B diminished. Knob 1 picks the chord type, one per click, and shows it on the keys
built on the root: triad, sus2, sus4, 6, 7, add9, 9, add11, 11, power (root, 5th, octave). A
"7" is a major 7th on C and a dominant 7th on G, as the scale has it. Strum and glide play each
new chord as a strum.

## Stack, strum and glide

- **Stack** (notes mode) adds scale notes: right of centre above the highest held note, left of
  centre below the lowest. Holding C E G in C major with 3rds gives B, D, F above, or A, F, D
  below. Held keys get voices first: holding 3 keys leaves room for 4 stacked notes. Nothing is
  stacked while no keys are held. Chompi key + press knob 1 steps the interval.
- **Strum** brings in notes that start together one at a time: keys pressed within 30 ms of
  each other plus the notes the stack adds to them. With strum up, the first note waits those
  30 ms too, so the order is right even if you press the top key first.
- **Glide** (strum off centre only): each strummed note starts at the pitch of the note before it
  and slides to its own, within the strum gap; fully right, each arrives just as the next starts.
  A stacked note that is already sounding also slides when you change the chord.

## Input threshold and freeze

- **Input threshold:** the harmonies only sound while you sing. When your voice has stayed 6 dB
  under it for 150 ms they fade with the release time, even with keys held; when it comes back
  over, the held chord strums in again. Set it just above the room noise. With release fully
  right they never fade, like the original SING.
- **Freeze** (turn it on first: Chompi key + press knob 3): push your voice over the freeze
  threshold and CHORALE records the next 0.3 s; the harmonies then loop that recording instead of
  following your voice, even while you keep singing quietly. Push over again to record a new
  0.3 s, which crossfades in. Let go of every note, or turn freeze off, to go back to live. Under
  the input threshold a freeze fades with the release like live harmonies, so with release fully
  right it holds. Set the threshold clearly above your normal singing, so only deliberate pushes
  freeze. Works with the switch either way.

## Filter envelope

Turn it on first (Chompi key + press knob 2), then lower the cutoff. One low-pass filters your
voice and the harmonies, before the effects and looper. Each time a harmony note enters (a key,
a stacked note, each step of a strum, a new phrase through the input threshold), the cutoff
sweeps up to fully open and back down.

## Input and resample

Hold the Chompi key and press a black key below the middle C: **F#** built-in mic, **G#** line in
(plugging into the input jack selects it), **A#** resample. These keys, and C# / D# above the middle C (effects before or after the looper), keep their
menu jobs instead of setting the root; every note also has a free key in the other octave.

In **resample** mode the harmonies play from the **loop** instead of your voice: record a loop,
select resample, and each held key adds a pitch-shifted copy of it, with stack, strum, glide,
freeze, the thresholds and the filter all working on the loop. Dry/wet then sets the original loop
against its harmonies, and input gain how hard the loop drives them. Your mic isn't heard in
resample mode.

## Lights

- **Keys:** held keys magenta, stacked and chord notes gold while sounding, the root dim amber.
  Turning the stack knob shows how many notes are stacked for a moment, one white key each from
  the middle C; in chord mode, turning knob 1 shows the chord type built on the root.
- **Knob rings** brighten as each control turns up: stack warm white (coral below, gold above); chord type one colour each around the wheel;
  strum green up, dark orange down; glide teal; attack yellow, release deep orange; filter attack
  purple, decay blue, cutoff green, resonance pale yellow; reverb blue, bitcrush red, doubler and
  spread white, compression peach.
- **While you hold the Chompi key:** the keys show the scale (root bright amber, scale notes dim)
  and the knob 1 ring the stack interval (3rds gold, 4ths amber, 5ths coral, 6ths rose, octaves
  warm white). The knob 2 ring shows the input
  threshold live (magenta over it, dim under it), the knob 3 ring the freeze (white recording,
  magenta over the threshold). Pressing knob 2 or 3 shows white for on, dim white for off.
- **Chompi key:** magenta in chord mode; it breathes while a freeze holds.

## Tips

- **Use headphones.** The built-in mic and speaker feed back.
- Both thresholds are measured after the input gain: change the gain and you may need to reset
  them.
- With the built-in mic, CHORALE dips the mic for 35 ms from the first contact of any button
  (keys, record, play, knob presses), and hears the mic 2 ms late so the dip covers the click
  from its start. That reduces the clicks but doesn't remove them; a mic on the input jack
  avoids them entirely.

## Install

With the [multi-firmware launcher](https://github.com/sfaber02/CHOMPI/releases/latest) v1.1 or
later, send the `.bin` to a free slot from <https://ugrossek.github.io/CHOMPI/>. On a stock card,
put it in the card root as the only `.bin`, like any firmware update.

CHORALE keeps its files (`options.json`) in a `/CHORALE` folder on the card if there is one,
otherwise in the card root.

## Known limits

- About 30–45 ms of latency on voices shifted up; voices on the middle C have none.
- Key clicks from the built-in mic are reduced, not gone.
- On some units the LEDs can be heard as a faint whine through the built-in mic.
- A freeze loops 0.3 s of whatever you sang: steady vowels loop smoothly, slides and vibrato less so.
- Tested on one unit.

## How it works

[`code/src/Harmonizer.h`](code/src/Harmonizer.h): each voice runs the input through its own copy
of the WSOLA pitch shifter from the
[TAPE pitch-shift experiment](https://github.com/ugrossek/CHOMPI/tree/true-pitch-shift/firmware/chompi-tape#pitch-shift-experiment-this-fork)
([`code/src/PitchShifter.h`](code/src/PitchShifter.h)), at a ratio of 2^(semitones/12). The
filter envelope is in [`code/src/FilterEnv.h`](code/src/FilterEnv.h).

## Building

Toolchain: GNU Arm Embedded 10.3-2021.10, as for TAPE (newer compilers can break the SD card).
The prebuilt libraries TAPE ships with (libDaisy, DaisySP, coreJSON) are in `code/libs`;
override with `make CHOMPI_LIBS=/path/to/libs`:

```bash
cd code/src
PATH=/path/to/gcc-arm-none-eabi-10.3-2021.10/bin:$PATH make -j8
```

Output is `code/src/build/CHORALE.bin`.

## Written with AI

SING, and the changes that make CHORALE, were written together with Claude, Anthropic's AI
assistant, and tested on one CHOMPI. Unofficial, experimental software, provided as-is with no
warranty (see [`LICENSE`](LICENSE)); use it at your own risk.
