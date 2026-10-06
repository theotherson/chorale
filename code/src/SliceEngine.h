#pragma once
#include <cmath>
#include "daisysp.h"
#include "SamplePlayer.h"

namespace chompi
{
    /** CHORALE's slice engine: TEMPO's vocal chops, on the looper's loop.
     *
     *  The loop (start and end window set by knobs) is cut into 16 equal
     *  slices, one per white key from the lowest C; the top C alternates
     *  between the last two, as in TEMPO. A key plays its slice once, through
     *  an attack / release envelope, at the pitch knob's speed. Up to 8 at a
     *  time; the oldest is taken when all are busy. The loop is read where it
     *  lies, so overdubs show up in the slices straight away.
     */
    class SliceEngine
    {
      public:
        static constexpr int kVoices = 8;
        static constexpr int kSlices = 16;

        void Init(float samplerate)
        {
            sr_ = samplerate;
            for (int v = 0; v < kVoices; v++)
            {
                Voice &vo = voices_[v];
                vo.player.Init(samplerate);
                vo.player.setLoop(false);
                vo.player.setFrequency(440.f); // speed 1, see SetPitch()
                vo.env.Init(samplerate);
                vo.env.SetSustainLevel(1.f);
                vo.key  = -1;
                vo.gate = false;
                vo.age  = 0;
            }
            mem_    = nullptr;
            frames_ = 0;
            toggle_ = false;
            clock_  = 0;
            SetPitch(.5f);
            SetVolume(.6f);
            SetStart(0.f);
            SetEnd(1.f);
            SetAttack(0.f);
            SetRelease(.2f);
            vol_ = vol_target_;
        }

        /** the loop, each block: interleaved 16-bit stereo, frames long */
        void SetSource(int16_t *mem, size_t frames)
        {
            mem_    = mem;
            frames_ = frames;
        }
        bool HasSource() const { return mem_ != nullptr && frames_ >= kSlices * 64; }

        /** key: hardware key id; note: MIDI note (white keys play slices) */
        void NoteOn(int key, int note)
        {
            const int s = SliceFor(note);
            if (s < 0 || !HasSource())
                return;

            Voice *v = nullptr;
            for (int i = 0; i < kVoices && !v; i++) // retrigger the same key
                if (voices_[i].key == key)
                    v = &voices_[i];
            for (int i = 0; i < kVoices && !v; i++)
                if (!voices_[i].gate && !voices_[i].env.IsRunning())
                    v = &voices_[i];
            if (!v) // the oldest
            {
                v = &voices_[0];
                for (int i = 1; i < kVoices; i++)
                    if (voices_[i].age < v->age)
                        v = &voices_[i];
            }

            size_t first, len;
            SliceBounds(s, first, len);
            v->player.setSample(mem_ + 2 * first, len);
            v->player.setGlobalFrequency(speed_);
            v->player.resetPlayer(false);
            v->key  = key;
            v->gate = true;
            v->age  = ++clock_;
        }

        void NoteOff(int key)
        {
            for (int i = 0; i < kVoices; i++)
                if (voices_[i].key == key)
                    voices_[i].gate = false;
        }

        void AllOff()
        {
            for (int i = 0; i < kVoices; i++)
                voices_[i].gate = false;
        }

        /** a key whose slice is sounding */
        bool Playing(int key) const
        {
            for (int i = 0; i < kVoices; i++)
                if (voices_[i].key == key && voices_[i].gate)
                    return true;
            return false;
        }

        /** adds the slices into l / r */
        void Process(float *l, float *r, size_t size)
        {
            for (size_t i = 0; i < size; i++)
            {
                vol_ += (vol_target_ - vol_) * .001f;
                float sl = 0.f, sr = 0.f;
                for (int v = 0; v < kVoices; v++)
                {
                    Voice &vo = voices_[v];
                    if (!vo.gate && !vo.env.IsRunning())
                        continue;
                    float a = 0.f, b = 0.f;
                    vo.player.PopStereoSamps(&a, &b);
                    const float e = vo.env.Process(vo.gate);
                    sl += a * e;
                    sr += b * e;
                }
                l[i] += sl * vol_;
                r[i] += sr * vol_;
            }
        }

        /* ---- knobs, each 0..1 (slice mode, knobs 1-3) ------------------ */

        /** knob 1, page 1: speed and pitch, .5 = as recorded, an octave
         *  each way */
        void SetPitch(float v)
        {
            speed_ = powf(2.f, (v - .5f) * 2.f);
            for (int i = 0; i < kVoices; i++)
                voices_[i].player.setGlobalFrequency(speed_);
        }
        /** knob 1, page 2 */
        void SetVolume(float v) { vol_target_ = v * v * 1.5f; }
        /** knobs 2 / 3, page 1: the part of the loop that is sliced */
        void SetStart(float v) { start_ = v; }
        void SetEnd(float v) { end_ = v; }
        /** knobs 2 / 3, page 2: 1 ms .. 1 s, 5 ms .. 2 s */
        void SetAttack(float v)
        {
            for (int i = 0; i < kVoices; i++)
                voices_[i].env.SetAttackTime(.001f * powf(1000.f, v));
        }
        void SetRelease(float v)
        {
            for (int i = 0; i < kVoices; i++)
                voices_[i].env.SetReleaseTime(.005f * powf(400.f, v));
        }

      private:
        struct Voice
        {
            samplePlayer  player;
            daisysp::Adsr env;
            int           key;
            bool          gate;
            uint32_t      age;
        };

        /** the slice a key plays: white keys from the lowest C, the top C
         *  the last two in turn; -1 for black keys */
        int SliceFor(int note)
        {
            static const int8_t kWhite[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
            const int n = note - 48;
            if (n < 0 || n > 24 || kWhite[n % 12] < 0)
                return -1;
            const int w = kWhite[n % 12] + 7 * (n / 12); // 0..14
            if (w < 14)
                return w;
            toggle_ = !toggle_;
            return toggle_ ? 14 : 15;
        }

        void SliceBounds(int s, size_t &first, size_t &len) const
        {
            float a = start_, b = end_;
            if (b < a + .02f)
                b = a + .02f;
            if (b > 1.f)
            {
                b = 1.f;
                a = b - .02f;
            }
            const size_t w0 = size_t(a * float(frames_));
            const size_t w1 = size_t(b * float(frames_));
            len   = (w1 - w0) / kSlices;
            first = w0 + size_t(s) * len;
            if (len < 1)
                len = 1;
        }

        Voice    voices_[kVoices];
        int16_t *mem_;
        size_t   frames_;
        float    sr_, speed_, vol_, vol_target_, start_, end_;
        bool     toggle_;
        uint32_t clock_;
    };

} // namespace chompi
