#pragma once
#include <cstdint>

namespace chompi
{
    /** CHORALE's sequencer, after TEMPO's ArpeggiatorSequencer.
     *
     *  It keeps the keys you hold (or have latched), and on each clock step
     *  picks the next one by the pattern mode, minus the rests of the rest
     *  pattern. The engine plays what it picks (a slice, a harmony note, or
     *  a chord) and lets it go after part of the step (see DSPEngine.h).
     *
     *  Modes, as TEMPO: sequence (the order the keys were pressed), arp up,
     *  arp down, arp up and down, random. Rest patterns: TEMPO's five.
     *
     *  Latch: the held keys stay in after they are let go; the next key
     *  pressed after all are up starts a new set. Called from the audio
     *  callback only.
     */
    class Sequencer
    {
      public:
        enum Mode { SEQUENCE, ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM, kNumModes };
        static constexpr int kNumRests = 5;
        static constexpr int kMaxKeys  = 16;

        void Init()
        {
            n_ = 0;
            held_ = 0;
            playing_ = latched_ = false;
            mode_ = SEQUENCE;
            rest_ = 0;
            idx_ = -1;
            dir_ = 1;
            step_ = 0;
            rand_ = 22222u;
        }

        /* ---- keys -------------------------------------------------------- */

        void KeyDown(int key, float semis)
        {
            if (latched_ && held_ == 0)
                n_ = 0; // a new latched set
            held_++;
            for (int i = 0; i < n_; i++)
                if (keys_[i].key == key)
                    return;
            if (n_ < kMaxKeys)
                keys_[n_++] = {key, semis};
        }

        void KeyUp(int key)
        {
            if (held_ > 0)
                held_--;
            if (latched_)
                return;
            Remove(key);
        }

        void Clear()
        {
            n_ = 0;
            idx_ = -1;
        }

        bool InSeq(int key) const
        {
            for (int i = 0; i < n_; i++)
                if (keys_[i].key == key)
                    return true;
            return false;
        }

        /* ---- transport --------------------------------------------------- */

        void SetPlay(bool on)
        {
            playing_ = on;
            idx_  = -1;
            dir_  = 1;
            step_ = 0;
        }
        bool Playing() const { return playing_; }

        /** the wheel's click: latch on; off drops the keys no longer held */
        void ToggleLatch()
        {
            latched_ = !latched_;
            if (!latched_ && held_ == 0)
                Clear();
        }
        bool Latched() const { return latched_; }

        void NextMode() { mode_ = (mode_ + 1) % kNumModes; idx_ = -1; dir_ = 1; }
        int  GetMode() const { return mode_; }
        void StepRest(int d) { rest_ = (rest_ + (d > 0 ? 1 : kNumRests - 1)) % kNumRests; }
        int  Rest() const { return rest_; }

        /** a clock step: the key to play (key, semis), or false for a rest
         *  or nothing to play */
        bool Step(int &key, float &semis)
        {
            static const bool kRests[kNumRests][20] = {
                {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
                {1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0,1,1,1,0},
                {1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0,1,1,0,0},
                {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0},
                {1,0,1,1,0,1,0,1,1,0,1,0,1,1,0,1,0,1,1,0}};
            const bool play = kRests[rest_][step_ % 20];
            step_++;
            if (!playing_ || n_ == 0 || !play)
                return false;

            /* the keys in this mode's order */
            Key order[kMaxKeys];
            for (int i = 0; i < n_; i++)
                order[i] = keys_[i];
            if (mode_ != SEQUENCE && mode_ != ARP_RANDOM)
                for (int a = 1; a < n_; a++) // by pitch
                    for (int b = a; b > 0 && order[b].semis < order[b - 1].semis; b--)
                    {
                        const Key t = order[b]; order[b] = order[b - 1]; order[b - 1] = t;
                    }

            switch (mode_)
            {
                case ARP_DOWN:
                    idx_ = idx_ <= 0 ? n_ - 1 : idx_ - 1;
                    break;
                case ARP_UPDOWN:
                    if (n_ == 1)
                        idx_ = 0;
                    else
                    {
                        idx_ += dir_;
                        if (idx_ >= n_) { dir_ = -1; idx_ = n_ - 2; }
                        if (idx_ < 0)   { dir_ = 1;  idx_ = n_ > 1 ? 1 : 0; }
                    }
                    break;
                case ARP_RANDOM:
                    rand_ = rand_ * 1664525u + 1013904223u;
                    idx_ = int((rand_ >> 16) % uint32_t(n_));
                    break;
                default: // sequence, arp up
                    idx_ = (idx_ + 1) % n_;
                    break;
            }
            if (idx_ >= n_ || idx_ < 0)
                idx_ = 0;
            key   = order[idx_].key;
            semis = order[idx_].semis;
            return true;
        }

      private:
        struct Key
        {
            int   key;
            float semis;
        };

        void Remove(int key)
        {
            int j = 0;
            for (int i = 0; i < n_; i++)
                if (keys_[i].key != key)
                    keys_[j++] = keys_[i];
            n_ = j;
        }

        Key      keys_[kMaxKeys];
        int      n_, held_, mode_, rest_, idx_, dir_;
        bool     playing_, latched_;
        uint32_t step_, rand_;
    };

} // namespace chompi
