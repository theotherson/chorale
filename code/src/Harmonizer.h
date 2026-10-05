#pragma once
#include <cmath>
#include "daisysp.h"
#include "MicFilter.h"
#include "PitchShifter.h"

namespace chompi
{
    /** doubler delay lines (L, R), in SDRAM: chompi_main.cpp */
    static constexpr size_t kChorusLen = 2048; // power of two, ~42 ms
    extern float chorus_mem[2][kChorusLen];

    /** freeze: two recordings, one looping while the next is taken
     *  (SDRAM: chompi_main.cpp). A loop is 0.3 s; 50 ms more is recorded
     *  for the crossfade at its seam. */
    static constexpr size_t kFreezeLoop = 14400; // 0.3 s at 48 kHz
    static constexpr size_t kFreezeXf   = 2400;  // 50 ms
    static constexpr size_t kFreezeLen  = kFreezeLoop + kFreezeXf;
    extern float freeze_mem[2][kFreezeLen];

    /** CHORALE: scales, as semitones above the tonic (Chompi key + knob 1) */
    static constexpr int kNumScales = 9;
    struct Scale
    {
        int n;        // notes per octave
        int step[7];
    };
    static constexpr Scale kScales[kNumScales] = {
        {7, {0, 2, 4, 5, 7, 9, 11}},  // major
        {7, {0, 2, 3, 5, 7, 8, 10}},  // natural minor
        {7, {0, 2, 3, 5, 7, 9, 10}},  // dorian
        {7, {0, 1, 3, 5, 7, 8, 10}},  // phrygian
        {7, {0, 2, 4, 6, 7, 9, 11}},  // lydian
        {7, {0, 2, 4, 5, 7, 9, 10}},  // mixolydian
        {7, {0, 2, 3, 5, 7, 8, 11}},  // harmonic minor
        {5, {0, 2, 4, 7, 9}},         // major pentatonic
        {5, {0, 3, 5, 7, 10}},        // minor pentatonic
    };

    /** CHORALE chord mode: chord types (knob 1), as scale steps above the
     *  pressed key's degree, so each chord's quality follows the scale.
     *  kOctave is the root an octave up. */
    static constexpr int kOctave    = 100;
    static constexpr int kNumChords = 10;
    static constexpr int kChordMax  = 6;
    struct Chord
    {
        int n;
        int step[kChordMax];
    };
    static constexpr Chord kChords[kNumChords] = {
        {3, {0, 2, 4}},              // triad
        {3, {0, 1, 4}},              // sus2
        {3, {0, 3, 4}},              // sus4
        {4, {0, 2, 4, 5}},           // 6
        {4, {0, 2, 4, 6}},           // 7
        {4, {0, 2, 4, 8}},           // add9
        {5, {0, 2, 4, 6, 8}},        // 9
        {4, {0, 2, 4, 10}},          // add11
        {6, {0, 2, 4, 6, 8, 10}},    // 11
        {3, {0, 4, kOctave}},        // power (root, 5th, octave)
    };

    /** SING's live harmonizer.
     *
     *  Every held key gets a voice that pitch-shifts the live input by its
     *  distance from the middle C, so holding C and E while singing gives the
     *  voice and a third above it. The C key itself is unshifted (the shifter
     *  fades to dry at a ratio of 1, so it adds no delay).
     *
     *  Knob 1 stacks extra notes on the held chord: diatonic steps in C
     *  major (3rds by default) above the highest held note, or below the
     *  lowest. Stacked notes take the voices the keys leave free.
     *
     *  Knob 1, page 2 strums: notes that start together (a chord played
     *  by hand, or notes the stack adds) come in one by one, a set gap
     *  apart: lowest first right of centre, highest first left of it.
     *  With glide (menu + knob 1, page 2), each strummed note enters at
     *  the pitch of the one before it and slides to its own within the
     *  strum gap, and a sounding stacked note slides to a new note when the
     *  chord changes.
     *
     *  An input gate (menu + knob 2, page 1) fades the harmonies with the
     *  release time while you're not singing, and strums the held chord in
     *  again each time your voice comes back over it. Release fully
     *  clockwise never fades them.
     *
     *  Freeze (menu + knob 3, page 1): singing over the freeze threshold
     *  records the next 0.3 s, and the harmonies then loop that recording
     *  instead of the live voice. Each new push over the threshold records
     *  again and crossfades to it. Letting go of every note, or turning
     *  the threshold off, goes back to live.
     *
     *  CHORALE: the stack and the chords follow a scale and tonic (menu).
     *  With the toggle on chord mode, a key plays a whole chord built on it
     *  (knob 1 picks the type); one chord at a time, the newest held key's.
     *
     *  Each voice runs one StereoPitchShifter on its own delay line
     *  (shift_mem, shift_ana in chompi_main.cpp). The dry voice is not added
     *  here: the engine's mic monitor provides it, routed by the monitor mode.
     *
     *  Init() sets every member, so the object may live in DTCM, which is
     *  not zeroed at start-up.
     */
    template <size_t kVoices>
    class Harmonizer
    {
    public:
        void Init(float samplerate)
        {
            for (size_t v = 0; v < kVoices; v++)
            {
                voices_[v].shifter.Init(shift_mem[v], shift_ana[v]);
                voices_[v].key  = -1;
                voices_[v].env  = 0.f;
                voices_[v].gate = false;
                voices_[v].semis = 0.f;
                voices_[v].pitch = 0.f;
                voices_[v].pan   = 0.f;
                voices_[v].strum    = false;
                voices_[v].start_at = 0;
                voices_[v].on       = false;
            }
            dcblock_.Init(samplerate);
            mic_filter_.Init(samplerate);
            sr_ = samplerate;
            chord_mode_ = false;
            chord_type_ = 0;
            chord_new_  = false;
            n_held_     = 0;
            scale_      = 0;
            tonic_      = 0;
            stack_ = 0;
            interval_idx_ = 0;
            dirty_ = false;
            strum_gap_ = 0;
            strum_age_ = kStrumWindow; // no strum open
            gate_env_  = 0.f;
            gate_open_ = false;
            gate_hold_ = 0;
            gate_fall_ = expf(-1.f / (.01f * samplerate)); // 10 ms peak decay
            entered_   = false;

            for (size_t b = 0; b < 2; b++)
                for (size_t i = 0; i < kFreezeLen; i++)
                    freeze_mem[b][i] = 0.f; // SDRAM is not zeroed at start-up
            ClearFreeze();

            /* the knob defaults in ui.h, so start-up matches the knobs even
               before a page is shown */
            SetStack(.5f);
            SetStrum(.5f);
            SetGlide(0.f);
            SetGate(.4f);
            SetFreeze(0.f);
            freeze_on_ = false; // starts off: Chompi key + press knob 3
            SetLevel(.75f);
            SetAttack(.1f);
            SetRelease(.5f);
            SetDoubler(0.f);
            SetSpread(0.f);

            for (size_t c = 0; c < 2; c++)
                for (size_t i = 0; i < kChorusLen; i++)
                    chorus_mem[c][i] = 0.f; // SDRAM is not zeroed at start-up
            chorus_w_  = 0;
            lfo_[0]    = 0.f;
            lfo_[1]    = .37f;

            /* key-click ducking, see Duck() */
            duck_      = 1.f;
            duck_hold_ = 0;
            duck_len_  = int(.035f * samplerate);
            duck_down_ = 1.f / (.0005f * samplerate);
            duck_up_   = 1.f / (.025f * samplerate);
        }

        /** Gain for the built-in mic, one value per sample of this block.
         *  The mic sits on the same board as the keys and hears every click,
         *  and a key press is exactly when a new voice starts. So for a moment
         *  after every button contact (KeyContact()) the mic is pulled down
         *  (0.5 ms), held, and brought back (25 ms). */
        /** Any button's contact changed, before debouncing (chompi_main.cpp,
         *  after ugrossek's sing-pitch SING): duck now, ~8 ms before the
         *  debounced press or release arrives, and for the record, play and
         *  knob buttons too, which don't play notes */
        void KeyContact() { duck_hold_ = duck_len_; }

        void Duck(float *gain, size_t size)
        {
            for (size_t i = 0; i < size; i++)
            {
                if (duck_hold_ > 0)
                {
                    duck_hold_--;
                    duck_ = duck_ - duck_down_ > kDuckFloor ? duck_ - duck_down_ : kDuckFloor;
                }
                else
                    duck_ = duck_ + duck_up_ < 1.f ? duck_ + duck_up_ : 1.f;
                gain[i] = duck_;
            }
        }

        /* ---- knobs, each 0..1 ------------------------------------------ */

        /** knob 1, page 1: notes stacked on the held chord, .5 = none.
         *  Each 1/12 is one note: above the highest held note to the right,
         *  below the lowest to the left. */
        void SetStack(float v)
        {
            stack_ = StackCount(v);
            dirty_ = true;
        }

        /** knob 1 value -> stacked notes, -kMaxStack..kMaxStack */
        static int StackCount(float v)
        {
            const int n = int(lroundf((v - .5f) * 2.f * kMaxStack));
            return n < -kMaxStack ? -kMaxStack : n > kMaxStack ? kMaxStack : n;
        }

        /** stacked interval, menu + knob 1: 3rds, 4ths, 5ths, 6ths, octaves */
        static constexpr int kNumIntervals = 5;
        void SetInterval(int idx)
        {
            interval_idx_ = idx < 0 ? 0 : idx >= kNumIntervals ? kNumIntervals - 1 : idx;
            dirty_ = true;
        }
        int Interval() const { return interval_idx_; }

        /** menu + knob 1: the scale, for the stack and the chords */
        void SetScale(int idx)
        {
            scale_ = idx < 0 ? 0 : idx >= kNumScales ? kNumScales - 1 : idx;
            dirty_ = true;
        }
        int GetScale() const { return scale_; }

        /** menu + a key: the tonic, 0..11 semitones above the middle C (your
         *  voice) */
        void SetTonic(int pc)
        {
            tonic_ = ((pc % 12) + 12) % 12;
            dirty_ = true;
        }
        int Tonic() const { return tonic_; }

        /** a note (semitones from middle C) is in the scale */
        bool InScale(int semis) const
        {
            const int rel = (((semis - tonic_) % 12) + 12) % 12;
            const Scale &sc = kScales[scale_];
            for (int i = 0; i < sc.n; i++)
                if (sc.step[i] == rel)
                    return true;
            return false;
        }

        /** toggle switch: chord mode (on) or notes mode; switching lets
         *  everything go */
        void SetChordMode(bool on)
        {
            if (on == chord_mode_)
                return;
            chord_mode_ = on;
            n_held_     = 0;
            AllOff();
        }
        bool ChordMode() const { return chord_mode_; }

        /** knob 1 in chord mode */
        void SetChordType(int idx)
        {
            chord_type_ = idx < 0 ? 0 : idx >= kNumChords ? kNumChords - 1 : idx;
            chord_new_  = true;
            dirty_      = true;
        }
        int ChordType() const { return chord_type_; }

        /** the notes of a chord of the current type built on root (semitones
         *  from middle C), into notes; returns how many fit the range */
        int ChordNotes(float root, float *notes) const
        {
            const Chord &ch  = kChords[chord_type_];
            const int    deg = Degree(root);
            int          n   = 0;
            for (int i = 0; i < ch.n && n < int(kVoices); i++)
            {
                const float s = ch.step[i] == kOctave ? DegreeSemis(deg) + 12.f
                                                      : DegreeSemis(deg + ch.step[i]);
                if (s >= kStackLow && s <= kStackHigh)
                    notes[n++] = s;
            }
            return n;
        }

        /** a stacked note is sounding at this many semitones from middle C
         *  (lit once it has entered the strum) */
        bool Stacked(float semis) const
        {
            for (size_t v = 0; v < kVoices; v++)
                if (voices_[v].gate && IsStackKey(voices_[v].key) && gate_open_
                    && voices_[v].semis == semis && !Waiting(voices_[v]))
                    return true;
            return false;
        }

        /** Re-voices the stack after a key or knob change. Called from the
         *  audio callback, like NoteOn(), so the voices change in one place. */
        void Refresh()
        {
            if (!dirty_)
                return;
            dirty_ = false;
            UpdateStack();
        }

        /** harmony volume, .5 = about as loud as the dry voice (fixed at
         *  .75 since knob 1 page 2 became strum; dry/wet is on knob 6) */
        void SetLevel(float v) { level_ = v * 2.f * kLevel; }

        /** knob 1, page 2: .5 = all at once; right of centre strums up
         *  (lowest first), left strums down, 0 .. 500 ms between notes */
        void SetStrum(float v)
        {
            const float d = (v - .5f) * 2.f;
            strum_gap_  = int(.5f * d * d * sr_);
            strum_down_ = d < 0.f;
        }

        /** menu + knob 1, page 2: glide, as a share of the strum gap: 0 =
         *  off, 1 = each note arrives just as the next one starts (longer
         *  smeared the notes into each other) */
        void SetGlide(float v) { glide_v_ = v; }
        float Glide() const { return glide_v_; }

        /** menu + knob 2, page 1: onset threshold. 0 = gate off (always
         *  open), else -70 .. -20 dB of the harmonizer input */
        void SetGate(float v)
        {
            gate_v_      = v;
            gate_thresh_ = v <= 0.f ? 0.f : powf(10.f, (-70.f + 50.f * v) / 20.f);
            gate_close_  = gate_thresh_ * .5f; // closes 6 dB under
        }
        float Gate() const { return gate_v_; }

        /** menu + knob 3, page 1: freeze threshold. 0 = freeze off (and
         *  back to live), else -60 .. -5 dB of the harmonizer input */
        void SetFreeze(float v)
        {
            freeze_v_      = v;
            freeze_thresh_ = v <= 0.f ? 0.f : powf(10.f, (-60.f + 55.f * v) / 20.f);
            if (v <= 0.f)
                frozen_ = capturing_ = false;
        }
        float Freeze() const { return freeze_v_; }

        /** menu + press knob 3: freeze on/off, keeping the threshold; off
         *  goes back to the live voice */
        void SetFreezeOn(bool on)
        {
            freeze_on_ = on;
            if (!on)
                frozen_ = capturing_ = false;
        }
        bool FreezeOn() const { return freeze_on_; }

        /** the voice is over the freeze threshold right now */
        bool OverFreeze() const
        {
            return freeze_on_ && freeze_thresh_ > 0.f && gate_env_ >= freeze_thresh_;
        }

        /** recording the next freeze */
        bool Capturing() const { return capturing_; }

        /** a harmony note has entered since the last call (for the filter
         *  envelope) */
        bool TakeEntered()
        {
            const bool e = entered_;
            entered_ = false;
            return e;
        }

        /** your voice is above the gate threshold (or the gate is off) */
        bool GateOpen() const { return gate_open_; }

        /** knob 2, page 2: 2 ms .. 500 ms */
        void SetAttack(float v) { attack_ = 1.f / (.002f * powf(250.f, v) * sr_); }

        /** knob 3, page 1: 20 ms .. 3 s; fully clockwise never fades */
        void SetRelease(float v)
        {
            release_ = v >= .99f ? 0.f : 1.f / (.02f * powf(150.f, v) * sr_);
        }

        /** the harmonies are looping a frozen recording */
        bool Frozen() const { return frozen_; }

        /** knob 3, page 1: chorus taps + slight detune per voice */
        void SetDoubler(float v)
        {
            doubler_ = v;
            for (size_t i = 0; i < kVoices; i++)
                UpdateRatio(voices_[i], i);
        }

        /** knob 2, page 1: held voices, ordered by pitch, go alternately left
         *  and right (lowest left), so highs and lows end up on both sides */
        void SetSpread(float v) { spread_ = v; }

        /** key: hardware key id (to match the note-off), semis: from middle C */
        void NoteOn(int key, float semis)
        {
            /* from silence the gate starts closed, so a chord pressed while
               singing strums in order once the voice opens it */
            if (!Active())
            {
                gate_open_ = false;
                gate_env_  = 0.f;
                ClearFreeze(); // a new chord from silence starts live
            }

            /* chord mode: the newest held key's chord plays (UpdateChord) */
            if (chord_mode_)
            {
                HeldRemove(key);
                if (n_held_ < kMaxHeld)
                {
                    held_[n_held_].key   = key;
                    held_[n_held_].semis = semis;
                    n_held_++;
                }
                chord_new_ = true;
                duck_hold_ = duck_len_;
                dirty_     = true;
                return;
            }

            Voice *v = Find(key);       // retrigger of a held key
            if (!v) v = FindFree();
            if (!v) v = FindQuietest(); // steal
            if (v->key != key || v->env <= 0.f)
                v->shifter.Reset();     // fresh history: fades in from silence
            if (v->key != key || !v->gate)
                v->pitch = semis;       // a new note starts on its own pitch
            v->key   = key;
            v->semis = semis;
            UpdateRatio(*v, size_t(v - voices_));
            v->gate  = true;
            StrumJoin(*v);
            duck_hold_ = duck_len_;
            dirty_     = true;
        }

        void NoteOff(int key)
        {
            duck_hold_ = duck_len_;
            dirty_     = true;
            if (chord_mode_)
            {
                /* letting go of the playing key goes back to the chord of
                   the key held before it, if any */
                const bool top = n_held_ > 0 && held_[n_held_ - 1].key == key;
                HeldRemove(key);
                if (top && n_held_ > 0)
                    chord_new_ = true;
                return;
            }
            if (Voice *v = Find(key))
                v->gate = false;
        }

        /** release every voice (they ring out with the release time) */
        void AllOff()
        {
            for (size_t v = 0; v < kVoices; v++)
                voices_[v].gate = false;
            dirty_ = true;
        }

        /** a key that is held down right now (not just ringing out); in
         *  chord mode, the key whose chord is playing */
        bool Held(int key) const
        {
            if (chord_mode_)
                return n_held_ > 0 && held_[n_held_ - 1].key == key;
            for (size_t v = 0; v < kVoices; v++)
                if (voices_[v].gate && voices_[v].key == key)
                    return true;
            return false;
        }

        bool Active() const
        {
            for (size_t v = 0; v < kVoices; v++)
                if (voices_[v].gate || voices_[v].env > 0.f)
                    return true;
            return false;
        }

        /** Adds the harmony voices into outl/outr.
         *  @param in    mono input, already scaled (mic or summed line in)
         *  @param mic   true: apply the mic filter as TAPE's monitor does */
        void Process(const float *in, bool mic, float *outl, float *outr, size_t size)
        {
            /* the shifters share a per-block budget for their splice search */
            StereoPitchShifter::NewBlock(size);

            float in_[size]; /* one audio block, 48 samples */
            for (size_t i = 0; i < size; i++)
            {
                float x = dcblock_.Process(in[i]);
                if (mic)
                    x = mic_filter_.Process(x);
                in_[i] = x;
            }

            /* input gate: opens the moment the voice passes the threshold,
               closes once it has stayed 6 dB under it for kGateHold */
            const bool was_open = gate_open_;
            for (size_t i = 0; i < size; i++)
            {
                const float a = fabsf(in_[i]);
                gate_env_ = a > gate_env_ ? a : gate_env_ * gate_fall_;
            }
            if (gate_thresh_ <= 0.f || gate_env_ >= gate_thresh_)
            {
                gate_open_ = true;
                gate_hold_ = kGateHold;
            }
            else if (gate_env_ < gate_close_)
            {
                gate_hold_ -= int(size);
                if (gate_hold_ <= 0)
                {
                    gate_hold_ = 0;
                    gate_open_ = false;
                }
            }
            if (gate_open_ && !was_open)
                StrumAll();

            /* freeze: a push over the threshold records the next 0.3 s (plus
               the seam crossfade); it re-arms once the voice is 6 dB under */
            if (freeze_on_ && freeze_thresh_ > 0.f)
            {
                if (freeze_armed_ && !capturing_ && gate_env_ >= freeze_thresh_)
                {
                    capturing_    = true;
                    freeze_armed_ = false;
                    cap_n_        = 0;
                    cap_buf_      = frozen_ ? 1 - play_buf_ : play_buf_;
                }
                else if (gate_env_ < freeze_thresh_ * .5f)
                    freeze_armed_ = true;
            }

            for (size_t i = 0; i < size; i++)
            {
                if (capturing_)
                {
                    freeze_mem[cap_buf_][cap_n_] = in_[i];
                    if (++cap_n_ >= kFreezeLen)
                    {
                        capturing_ = false;
                        if (frozen_) // crossfade from the loop playing now
                        {
                            old_buf_ = play_buf_;
                            old_pos_ = play_pos_;
                            swap_    = kFreezeSwap;
                        }
                        play_buf_ = cap_buf_;
                        play_pos_ = 0;
                        frozen_   = true;
                    }
                }

                /* the harmonies hear the loop instead of the live voice,
                   crossfaded over ~20 ms either way */
                freeze_mix_ += ((frozen_ ? 1.f : 0.f) - freeze_mix_) * kFreezeMixCoef;
                if (freeze_mix_ > .0001f)
                {
                    float y = LoopSample(play_buf_, play_pos_);
                    if (swap_ > 0)
                    {
                        const float t = 1.f - float(swap_) / float(kFreezeSwap);
                        y = y * sqrtf(t) + LoopSample(old_buf_, old_pos_) * sqrtf(1.f - t);
                        swap_--;
                    }
                    in_[i] += (y - in_[i]) * freeze_mix_;
                }
            }

            float hl[size], hr[size];
            for (size_t i = 0; i < size; i++)
                hl[i] = hr[i] = 0.f;

            /* glide: each voice's pitch slides towards its note, once per
               block (glide_s is about the time to get there) */
            const float glide_s = glide_v_ * float(strum_gap_) / sr_; // seconds
            const float glide_k = glide_s <= 0.f ? 1.f
                                  : 1.f - expf(-3.f * float(size) / (glide_s * sr_));

            for (size_t v = 0; v < kVoices; v++)
            {
                Voice &vo = voices_[v];
                if (!vo.gate && vo.env <= 0.f)
                    continue;

                if (vo.pitch != vo.semis)
                {
                    vo.pitch += (vo.semis - vo.pitch) * glide_k;
                    if (fabsf(vo.semis - vo.pitch) < .005f)
                        vo.pitch = vo.semis;
                    UpdateRatio(vo, v);
                }

                /* constant-power pan, glides so a voice that changes side
                   when a key is added doesn't jump */
                vo.pan += (PanTarget(v) - vo.pan) * .15f;
                const float a  = (vo.pan + 1.f) * .7853982f; // 0..pi/2
                const float gl = cosf(a) * 1.4142136f * level_;
                const float gr = sinf(a) * 1.4142136f * level_;

                for (size_t i = 0; i < size; i++)
                {
                    /* a strummed note waits for its turn; the input gate
                       silences everything while you're not singing */
                    const bool on = vo.gate && gate_open_
                                    && (!vo.strum || strum_age_ + int(i) >= vo.start_at);
                    if (on && !vo.on)
                    {
                        entered_ = true;
                        /* glide: a strummed note enters on the note before it */
                        if (glide_v_ > 0.f && strum_gap_ > 0 && vo.strum)
                            if (const Voice *p = StrumBefore(vo))
                            {
                                vo.pitch = p->semis;
                                UpdateRatio(vo, v);
                            }
                    }
                    vo.on = on;
                    if (on)
                        vo.env = vo.env + attack_ < 1.f ? vo.env + attack_ : 1.f;
                    else
                        vo.env = vo.env - release_ > 0.f ? vo.env - release_ : 0.f;

                    float l = in_[i], r = in_[i];
                    vo.shifter.Process(vo.ratio, &l, &r);
                    hl[i] += l * vo.env * gl;
                    hr[i] += r * vo.env * gr;
                }
                if (!vo.gate && vo.env <= 0.f)
                    vo.key = -1;
            }

            if (strum_age_ < kStrumMaxAge)
                strum_age_ += int(size);

            /* doubler: two slowly wandering taps (12 and 17 ms, +-3 ms),
               one per side, mixed in by the knob */
            const float lfo_inc = .35f / sr_;
            for (size_t i = 0; i < size; i++)
            {
                const float m = (hl[i] + hr[i]) * .5f;
                chorus_mem[0][chorus_w_] = m;

                float wet[2];
                for (size_t c = 0; c < 2; c++)
                {
                    lfo_[c] += lfo_inc * (c ? 1.3f : 1.f);
                    if (lfo_[c] >= 1.f)
                        lfo_[c] -= 1.f;
                    const float d = ((c ? .017f : .012f)
                                     + .003f * sinf(6.2831853f * lfo_[c])) * sr_;
                    const float rp = float(chorus_w_) - d;
                    const float fl = floorf(rp);
                    const size_t i0 = size_t(int(fl)) & (kChorusLen - 1);
                    const size_t i1 = (i0 + 1) & (kChorusLen - 1);
                    const float  fr = rp - fl;
                    wet[c] = chorus_mem[0][i0] + (chorus_mem[0][i1] - chorus_mem[0][i0]) * fr;
                }
                chorus_w_ = (chorus_w_ + 1) & (kChorusLen - 1);

                outl[i] += hl[i] + wet[0] * doubler_;
                outr[i] += hr[i] + wet[1] * doubler_;
            }
        }

    private:
        struct Voice
        {
            StereoPitchShifter shifter;
            int                key;
            float              ratio;
            float              semis;    // the note, from middle C
            float              pitch;    // where it sounds now (glides to semis)
            float              pan;
            float              env;
            bool               gate;
            bool               strum;    // part of the current strum
            int                start_at; // enters at this strum age (samples)
            bool               on;       // sounding (gate, strum turn) last sample
        };

        /** stacked notes use these keys, one per stack slot */
        static constexpr int kStackKey = 1000;
        static constexpr int kChordKey = 2000; // chord mode's notes, one per slot
        static bool IsStackKey(int key) { return key >= kStackKey; }

        /** Gives the held chord its stacked notes: |stack_| diatonic steps
         *  beyond its highest (or lowest) note, as many as free voices allow.
         *  A stack slot keeps its voice when the chord changes, gliding to
         *  the new note. */
        void UpdateStack()
        {
            if (chord_mode_)
            {
                UpdateChord();
                return;
            }

            float lo = 0.f, hi = 0.f;
            int   held = 0;
            for (size_t v = 0; v < kVoices; v++)
            {
                const Voice &vo = voices_[v];
                if (!vo.gate || vo.key < 0 || IsStackKey(vo.key))
                    continue;
                if (held == 0 || vo.semis < lo) lo = vo.semis;
                if (held == 0 || vo.semis > hi) hi = vo.semis;
                held++;
            }

            int want = held ? (stack_ < 0 ? -stack_ : stack_) : 0;
            if (want > int(kVoices) - held)
                want = int(kVoices) - held;
            const int dir  = stack_ < 0 ? -1 : 1;
            const int base = Degree(dir > 0 ? hi : lo);
            /* scale steps: 3rds .. 6ths, and an octave is the whole scale */
            const int kSteps[kNumIntervals] = {2, 3, 4, 5, kScales[scale_].n};

            float notes[kVoices];
            int   count = 0;
            for (; count < want; count++)
            {
                notes[count] = DegreeSemis(base + dir * kSteps[interval_idx_] * (count + 1));
                if (notes[count] < kStackLow || notes[count] > kStackHigh)
                    break;
            }

            /* slots no longer wanted ring out first, so their voices can
               go to a slot that lost its voice to a key press */
            for (size_t v = 0; v < kVoices; v++)
                if (IsStackKey(voices_[v].key) && voices_[v].key - kStackKey >= count)
                    voices_[v].gate = false; // chord slots too: none in notes mode

            for (int slot = 0; slot < count; slot++)
            {
                const int key = kStackKey + slot;
                Voice *v = Find(key);
                if (!v) v = FindFree();
                if (!v) v = FindReleasing();
                if (!v) break;
                if (v->key != key || v->env <= 0.f)
                    v->shifter.Reset();
                const bool starts = !v->gate || v->key != key;
                if (starts)
                    v->pitch = notes[slot];
                v->key   = key;
                v->semis = notes[slot];
                UpdateRatio(*v, size_t(v - voices_));
                v->gate  = true;
                if (starts) // a slot already sounding just glides to its new note
                    StrumJoin(*v);
            }
        }

        /** Chord mode: voices the newest held key's chord, one voice per
         *  note. A new chord (another key, or a new type) starts its notes
         *  again, so it strums; the same chord just keeps sounding. */
        void UpdateChord()
        {
            float notes[kVoices];
            const int count = n_held_ > 0 ? ChordNotes(held_[n_held_ - 1].semis, notes) : 0;

            for (size_t v = 0; v < kVoices; v++)
                if (voices_[v].key >= kChordKey && voices_[v].key - kChordKey >= count)
                    voices_[v].gate = false;

            for (int slot = 0; slot < count; slot++)
            {
                const int key = kChordKey + slot;
                Voice *v = Find(key);
                if (!v) v = FindFree();
                if (!v) v = FindReleasing();
                if (!v) break;
                if (v->key != key || v->env <= 0.f)
                    v->shifter.Reset();
                const bool starts = !v->gate || v->key != key || chord_new_;
                if (starts)
                    v->pitch = notes[slot];
                v->key   = key;
                v->semis = notes[slot];
                UpdateRatio(*v, size_t(v - voices_));
                v->gate  = true;
                if (starts)
                    StrumJoin(*v);
            }
            chord_new_ = false;
        }

        void HeldRemove(int key)
        {
            int j = 0;
            for (int i = 0; i < n_held_; i++)
                if (held_[i].key != key)
                    held_[j++] = held_[i];
            n_held_ = j;
        }

        /** Puts a starting note into the current strum, or opens a new one.
         *  A strum stays open for kStrumWindow after it opens (so a chord
         *  played by hand counts as one) and while notes are still waiting.
         *  Its notes enter in strum order, strum_gap_ apart, after the window. */
        void StrumJoin(Voice &v)
        {
            bool open = strum_age_ < kStrumWindow;
            for (size_t o = 0; o < kVoices && !open; o++)
                if (Waiting(voices_[o]))
                    open = true;
            if (!open)
            {
                for (size_t o = 0; o < kVoices; o++)
                    voices_[o].strum = false;
                strum_age_ = 0;
            }
            v.strum    = true;
            v.start_at = 0x7fffffff; // waiting, ranked below

            /* rank the notes that haven't entered yet by pitch, after the
               ones that have */
            for (size_t a = 0; a < kVoices; a++)
            {
                Voice &va = voices_[a];
                if (!Waiting(va))
                    continue;
                int rank = 0;
                for (size_t b = 0; b < kVoices; b++)
                {
                    const Voice &vb = voices_[b];
                    if (vb.strum && vb.gate && b != a && StrumsFirst(vb, b, va, a))
                        rank++;
                }
                /* with strum up, the first note waits out the chord window
                   too, so a chord played by hand is all in before the
                   order is fixed */
                va.start_at = (strum_gap_ > 0 ? kStrumWindow : 0) + rank * strum_gap_;
            }
        }

        /** The voice came back over the gate: every held note, stacked ones
         *  included, strums in again, in strum order. With strum up it waits
         *  out the chord window too, in case keys are still coming in. */
        void StrumAll()
        {
            strum_age_ = 0;
            for (size_t a = 0; a < kVoices; a++)
            {
                Voice &va = voices_[a];
                va.strum  = va.gate;
                if (!va.gate)
                    continue;
                int rank = 0;
                for (size_t b = 0; b < kVoices; b++)
                {
                    const Voice &vb = voices_[b];
                    if (vb.gate && b != a && StrumsFirst(vb, b, va, a))
                        rank++;
                }
                va.start_at = (strum_gap_ > 0 ? kStrumWindow : 0) + rank * strum_gap_;
            }
        }

        /** back to the live voice, nothing recorded */
        void ClearFreeze()
        {
            frozen_       = false;
            capturing_    = false;
            freeze_armed_ = true;
            freeze_mix_   = 0.f;
            cap_n_ = cap_buf_ = play_buf_ = old_buf_ = 0;
            play_pos_ = old_pos_ = 0;
            swap_     = 0;
        }

        /** One sample of a freeze loop, advancing pos. The loop is
         *  kFreezeLoop long; over its first kFreezeXf it crossfades in from
         *  what was recorded after its end, so the seam is smooth. */
        static float LoopSample(size_t buf, size_t &pos)
        {
            float y = freeze_mem[buf][pos];
            if (pos < kFreezeXf)
            {
                const float t = float(pos) / float(kFreezeXf);
                y = y * sqrtf(t) + freeze_mem[buf][kFreezeLoop + pos] * sqrtf(1.f - t);
            }
            if (++pos >= kFreezeLoop)
                pos = 0;
            return y;
        }

        /** voice b (index ib) enters before voice a (index ia): lower
         *  pitch first strumming up, higher first strumming down */
        bool StrumsFirst(const Voice &b, size_t ib, const Voice &a, size_t ia) const
        {
            if (b.semis == a.semis)
                return ib < ia;
            return strum_down_ ? b.semis > a.semis : b.semis < a.semis;
        }

        /** the note entering just before v in the current strum, if any */
        const Voice *StrumBefore(const Voice &v) const
        {
            const Voice *best = nullptr;
            for (size_t o = 0; o < kVoices; o++)
            {
                const Voice &vo = voices_[o];
                if (&vo == &v || !vo.strum || !vo.gate || vo.start_at >= v.start_at)
                    continue;
                if (!best || vo.start_at > best->start_at)
                    best = &vo;
            }
            return best;
        }

        /** in the strum, gated, and not yet entered */
        bool Waiting(const Voice &v) const
        {
            return v.strum && v.gate && strum_age_ < v.start_at;
        }

        /** scale degree at or below a note (semitones from middle C; a note
         *  outside the scale counts as the scale note below it) */
        int Degree(float semis) const
        {
            const Scale &sc  = kScales[scale_];
            const int    rel = int(lroundf(semis)) - tonic_;
            const int    oct = rel >= 0 ? rel / 12 : -((11 - rel) / 12);
            const int    pc  = rel - oct * 12;
            int          idx = 0;
            for (int i = 0; i < sc.n; i++)
                if (sc.step[i] <= pc)
                    idx = i;
            return oct * sc.n + idx;
        }

        /** semitones from middle C of a scale degree */
        float DegreeSemis(int deg) const
        {
            const Scale &sc  = kScales[scale_];
            const int    oct = deg >= 0 ? deg / sc.n : -((sc.n - 1 - deg) / sc.n);
            return float(tonic_ + oct * 12 + sc.step[deg - oct * sc.n]);
        }

        /** pitch = key, plus a few cents of alternating detune
         *  when the doubler is up, so stacked voices thicken */
        void UpdateRatio(Voice &v, size_t idx)
        {
            const float cents = (idx & 1 ? 1.f : -1.f) * doubler_ * 12.f;
            v.ratio = powf(2.f, (v.pitch + cents / 100.f) / 12.f);
        }

        /** -1..1 for voice v: its rank by pitch among the sounding voices,
         *  alternating sides; a lone voice stays in the middle */
        float PanTarget(size_t v) const
        {
            int rank = 0, count = 0;
            for (size_t o = 0; o < kVoices; o++)
            {
                if (!voices_[o].gate && voices_[o].env <= 0.f)
                    continue;
                count++;
                if (voices_[o].semis < voices_[v].semis
                    || (voices_[o].semis == voices_[v].semis && o < v))
                    rank++;
            }
            if (count < 2)
                return 0.f;
            return (rank & 1 ? 1.f : -1.f) * spread_;
        }

        Voice *Find(int key)
        {
            for (size_t v = 0; v < kVoices; v++)
                if (voices_[v].key == key)
                    return &voices_[v];
            return nullptr;
        }

        Voice *FindFree()
        {
            for (size_t v = 0; v < kVoices; v++)
                if (voices_[v].key < 0)
                    return &voices_[v];
            return nullptr;
        }

        /** the quietest voice ringing out, or nullptr */
        Voice *FindReleasing()
        {
            Voice *best = nullptr;
            for (size_t v = 0; v < kVoices; v++)
                if (!voices_[v].gate && (!best || voices_[v].env < best->env))
                    best = &voices_[v];
            return best;
        }

        /** for a key press: a voice ringing out, else a stacked note (keys
         *  come first; the stack shrinks to fit), else the first voice */
        Voice *FindQuietest()
        {
            if (Voice *v = FindReleasing())
                return v;
            for (size_t v = 0; v < kVoices; v++)
                if (IsStackKey(voices_[v].key))
                    return &voices_[v];
            return &voices_[0];
        }

        static constexpr float kLevel     = 1.4f;  /* about as loud as the dry voice */
        static constexpr float kDuckFloor = .03f;  /* -30 dB while a key clicks */
        static constexpr int   kMaxStack  = 6;     /* keys hold at least one voice */
        static constexpr int   kStrumWindow = 1440;    /* 30 ms at 48 kHz: one chord */
        static constexpr int   kStrumMaxAge = 1 << 28; /* the age stops counting here */
        static constexpr int   kGateHold    = 7200;    /* 150 ms under the threshold closes the gate */
        static constexpr size_t kFreezeSwap    = 960;   /* 20 ms from one freeze to the next */
        static constexpr float  kFreezeMixCoef = .001f; /* ~20 ms live <-> frozen */
        static constexpr float kStackLow  = -36.f; /* stacked notes stay within */
        static constexpr float kStackHigh = 24.f;  /* the shifter's range */

        Voice            voices_[kVoices];
        daisysp::DcBlock dcblock_;
        MicFilter        mic_filter_;
        float            attack_, release_;
        float            freeze_v_, freeze_thresh_, freeze_mix_; /* freeze */
        bool             frozen_, capturing_, freeze_armed_, freeze_on_;
        size_t           cap_n_, cap_buf_, play_buf_, play_pos_, old_buf_, old_pos_;
        int              swap_;
        bool             chord_mode_, chord_new_;          /* CHORALE */
        int              chord_type_, scale_, tonic_, n_held_;
        static constexpr int kMaxHeld = 8;
        struct Held_ { int key; float semis; } held_[kMaxHeld];
        float            sr_, level_, doubler_, spread_;
        int              stack_, interval_idx_;
        int              strum_gap_, strum_age_; /* samples */
        float            glide_v_;                /* glide: share of the strum gap */
        bool             strum_down_;
        float            gate_v_, gate_thresh_, gate_env_, gate_fall_;
        float            gate_close_;
        bool             gate_open_, entered_;
        int              gate_hold_;
        volatile bool    dirty_;
        float            lfo_[2];
        size_t           chorus_w_;
        float            duck_, duck_down_, duck_up_;
        int              duck_hold_, duck_len_;
    };

} // namespace chompi
