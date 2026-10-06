#pragma once
#include <cmath>
#include "daisysp.h"

namespace chompi
{
    /** SING's filter envelope: one low-pass on the live sound (your voice
     *  and the harmonies), before the effects and the looper.
     *
     *  Each time a harmony note enters, Trigger() sweeps the cutoff up
     *  towards fully open (attack) and back down to the cutoff (decay).
     *  With the cutoff fully open there is nothing to sweep, and the filter
     *  is bypassed.
     *
     *  Knob 2 / 3, page 2: attack / decay; with the Chompi key: cutoff /
     *  resonance. Init() sets every member.
     */
    class FilterEnv
    {
      public:
        /** the signals it filters: harmonies L/R, dry voice L/R */
        enum Channel { HARM_L, HARM_R, DRY_L, DRY_R, NUM_CHANNELS };

        void Init(float samplerate)
        {
            sr_ = samplerate;
            for (size_t c = 0; c < NUM_CHANNELS; c++)
            {
                svf_[c].Init(samplerate);
                svf_[c].SetDrive(0.f);
            }
            env_       = 0.f;
            attacking_ = false;
            enabled_   = true; // off at its default: the cutoff fully open
            SetAttack(0.f);
            SetDecay(.5f);
            SetCutoff(1.f);
            SetRes(0.f);
            active_ = false;
            for (size_t s = 0; s < kMaxSteps; s++)
                hz_[s] = 16000.f;
        }

        /** 1 ms .. 2 s */
        void SetAttack(float v) { attack_ = 1.f / (.001f * powf(2000.f, v) * sr_); }

        /** 10 ms .. 5 s, to about -60 dB */
        void SetDecay(float v) { decay_ = expf(-6.9f / (.01f * powf(500.f, v) * sr_)); }

        /** 40 Hz .. 16 kHz; fully open is off */
        void SetCutoff(float v) { cutoff_ = v; }

        void SetRes(float v)
        {
            res_ = v;
            for (size_t c = 0; c < NUM_CHANNELS; c++)
                svf_[c].SetRes(v * .9f);
        }

        /** menu + press knob 2: on/off, keeping the settings */
        void SetEnabled(bool on) { enabled_ = on; }
        bool Enabled() const { return enabled_; }

        /** a harmony note entered: sweep from where the envelope is now */
        void Trigger() { attacking_ = true; }

        /** Advances the envelope over a block and works out its cutoffs,
         *  one per kStep samples: once per block stepped audibly on fast
         *  sweeps. */
        void UpdateBlock(size_t size)
        {
            active_ = enabled_ && cutoff_ < .999f; // only the knob or the switch turns it off
            for (size_t s = 0; s * kStep < size && s < kMaxSteps; s++)
            {
                for (size_t i = 0; i < kStep; i++)
                {
                    if (attacking_)
                    {
                        env_ += attack_;
                        if (env_ >= 1.f)
                        {
                            env_       = 1.f;
                            attacking_ = false;
                        }
                    }
                    else
                        env_ *= decay_;
                }
                const float pos = cutoff_ + env_ * (1.f - cutoff_);
                hz_[s] = 40.f * powf(400.f, pos < .999f ? pos : .999f);
            }
        }

        /** Sample i of this block, filtered, or the sample itself with the
         *  cutoff knob fully open. The filters always run, so their state
         *  is current whenever the cutoff comes down. */
        float Process(Channel c, size_t i, float in)
        {
            if (active_ && i % kStep == 0 && i / kStep < kMaxSteps)
                svf_[c].SetFreq(hz_[i / kStep]);
            svf_[c].Process(in);
            return active_ ? svf_[c].Low() : in;
        }

      private:
        static constexpr size_t kStep     = 4;  // samples per cutoff update
        static constexpr size_t kMaxSteps = 32; // blocks up to 128 samples

        daisysp::Svf svf_[NUM_CHANNELS];
        float        hz_[kMaxSteps];
        float        sr_, attack_, decay_, cutoff_, res_, env_;
        bool         attacking_, active_, enabled_;
    };

} // namespace chompi
