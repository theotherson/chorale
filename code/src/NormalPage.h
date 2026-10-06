#pragma once

#include "hardware.h"
#include "DSPEngine.h"
#include "temp_led_stuff.h"

namespace chompi
{
        static const uint8_t cc_map[4][6] = {
            {20, 21, 22, 23, 24, 25},
            {28, 29, 30, 31, 0, 32},
            {0, 0, 0, 33, 0, 0},
            {0, 0, 0, 34, 0, 0}}; // SING: knob 4 page 4, doubler

        static const uint8_t key_map[40] = {
            0x01, /**< ENC_1_SW  page */
            0x02, /**< ENC_2_SW page */
            0x03, /**< ENC_3_SW page */
            0x00, /**< ENC_4_SW page */ /** TODO: was CC 16 out */
            0x04, /**< ENC_5_SW page */
            0x15, /**< KEY_26 chompi cc */
            0x00, /**< SW_TOG skip */
            0x31, /**< KEY_16 */
            0x32, /**< KEY_2 */
            0x34, /**< KEY_3 */
            0x35, /**< KEY_4 */
            0x37, /**< KEY_5 */
            0x33, /**< KEY_17 */
            0x36, /**< KEY_18 */
            0x38, /**< KEY_19 */
            0x30, /**< KEY_1 */
            0x39, /**< KEY_6 */
            0x3b, /**< KEY_7 */
            0x3c, /**< KEY_8 */
            0x3e, /**< KEY_9 */
            0x40, /**< KEY_10 */
            0x3a, /**< KEY_20 */
            0x3d, /**< KEY_21 */
            0x3f, /**< KEY_22 */
            0x41, /**< KEY_11 */
            0x43, /**< KEY_12 */
            0x45, /**< KEY_13 */
            0x47, /**< KEY_14 */
            0x48, /**< KEY_15 */
            0x42, /**< KEY_23 */
            0x44, /**< KEY_24 */
            0x46, /**< KEY_25 */
            0x05, /**< ENC_6_SW page */
            0x17, /**< KEY_27 play cc */
            0x18, /**< KEY_28 loop cc */
            0x00, /**< NC_1 skip */
            0x00, /**< NC_2 skip */
            0x00, /**< NC_3 skip */
            0x00, /**< NC_4 skip */
            0x00, /**< NC_5 skip */
        };

        static const uint8_t led_map[40]{
            2,    /**< ENC_1_SW  page */
            3,    /**< ENC_2_SW page */
            4,    /**< ENC_3_SW page */
            1,    /**< ENC_4_SW cc */
            0,    /**< ENC_5_SW page */
            0,    /**< KEY_26 chompi cc */
            0x00, /**< SW_TOG skip */
            0,    /**< KEY_16 */
            23,   /**< KEY_2 */
            22,   /**< KEY_3 */
            21,   /**< KEY_4 */
            20,   /**< KEY_5 */
            1,    /**< KEY_17 */
            2,    /**< KEY_18 */
            3,    /**< KEY_19 */
            24,   /**< KEY_1 */
            19,   /**< KEY_6 */
            18,   /**< KEY_7 */
            17,   /**< KEY_8 */
            16,   /**< KEY_9 */
            15,   /**< KEY_10 */
            4,    /**< KEY_20 */
            5,    /**< KEY_21 */
            6,    /**< KEY_22 */
            14,   /**< KEY_11 */
            13,   /**< KEY_12 */
            12,   /**< KEY_13 */
            11,   /**< KEY_14 */
            10,   /**< KEY_15 */
            7,    /**< KEY_23 */
            8,    /**< KEY_24 */
            9,    /**< KEY_25 */
            9,    /**< ENC_6_SW page */
            7,    /**< KEY_27 play cc */
            8,    /**< KEY_28 loop cc */
            0x00, /**< NC_1 skip */
            0x00, /**< NC_2 skip */
            0x00, /**< NC_3 skip */
            0x00, /**< NC_4 skip */
            0x00, /**< NC_5 skip */
        };

    // .00787 ~= what midi was. 1 / 127
    static const float kEncoderFineStep = .003f;
    static const float kEncoderCoarseStep = .01f;
    static const float kRecDim = .7f;

    uint8_t led_pth_cache[kNumPthLeds][3]; /**< RGB data */
    uint8_t led_smt_cache[kNumSmtLeds][3]; /**< RGB data */

    static const float white[3] = {1.f, 1.f, 1.f};
    static const float red[3] = {1.f, 0.f, 0.f};
    static const float orange[3] = {1.f, .6f, .24f};
    static const float yellow[3] = {1.f, .95f, 0.05f};
    static const float green[3] = {0.f, 1.f, 0.f};
    static const float teal[3] = {.14f, 1.f, .92f};
    static const float med_blue[3] = {0.f, .84f, 1.f};
    static const float blue[3] = {0.f, 0.f, 1.f};
    static const float purple[3] = {.58f, .05f, 1.f};
    static const float pink[3] = {1.f, .36f, .62f};
    static const float dark_orange[3] = {.77f, .38f, .06f};
    static const float yellow_green[3] = {.706f, 1.f, 0.f};

    /* SING: "warm stage" palette, to tell it apart from TAPE */
    static const float sing_magenta[3] = {1.f, .12f, .47f};
    static const float sing_coral[3]   = {1.f, .42f, .30f};
    static const float sing_amber[3]   = {1.f, .55f, 0.f};
    static const float sing_gold[3]    = {1.f, .78f, .10f};
    static const float sing_rose[3]    = {1.f, .45f, .60f};
    static const float sing_warm[3]    = {1.f, .85f, .65f};
    static const float pale_yellow[3]  = {1.f, .95f, .45f}; // filter resonance
    static const float deep_orange[3]  = {1.f, .3f, 0.f};   // amp release

    /** knob 1 stack ring: warm white with nothing stacked, coral below,
     *  gold above, brighter the more notes */
    static inline void SingStackColour(float v, float &r, float &g, float &b);

    /** colour between a and b at t, into r/g/b */
    static inline void SingMix(const float *a, const float *b, float t,
                               float &r, float &g, float &bl)
    {
        r  = a[0] + (b[0] - a[0]) * t;
        g  = a[1] + (b[1] - a[1]) * t;
        bl = a[2] + (b[2] - a[2]) * t;
    }

    /** the stacked interval, one colour each (3rds .. octaves); also the
     *  chord voicings in the menu */
    static const float kIntervalColours[5][3] = {
        {1.f, .78f, .10f}, // 3rds: gold
        {1.f, .55f, 0.f},  // 4ths: amber
        {1.f, .42f, .30f}, // 5ths: coral
        {1.f, .45f, .60f}, // 6ths: rose
        {1.f, .85f, .65f}, // octaves: warm white
    };

    /** knob 1 (notes mode): dim warm white with nothing stacked, else the
     *  interval's colour, full above, dimmer below */
    static inline void SingStackColour(float v, float &r, float &g, float &b)
    {
        const int p = Harmonizer<kMaxPoly>::StackPos(v);
        if (p == 0)
        {
            r = sing_warm[0] * .4f; g = sing_warm[1] * .4f; b = sing_warm[2] * .4f;
            return;
        }
        const float *c   = kIntervalColours[(p < 0 ? -p : p) - 1];
        const float  lvl = p > 0 ? 1.f : .35f;
        r = c[0] * lvl; g = c[1] * lvl; b = c[2] * lvl;
    }

    /** CHORALE: knobs 1-3 in slice mode, in TEMPO's colours */
    static inline void SliceKnobColour(int knob, int page, float v, float &r, float &g, float &b)
    {
        switch(page * 3 + knob)
        {
            case 0: // pitch: blue at the centre, through green and yellow to red outwards
            {
                const float idx = v < .5f ? (.5f - v) * 2.f : (v - .5f) * 2.f;
                r = color_quad_xfade(med_blue[0], green[0], yellow[0], red[0], idx);
                g = color_quad_xfade(med_blue[1], green[1], yellow[1], red[1], idx);
                b = color_quad_xfade(med_blue[2], green[2], yellow[2], red[2], idx);
                return;
            }
            case 1: SingMix(yellow, orange, v, r, g, b); return; // start
            case 2: SingMix(orange, red, v, r, g, b); return;    // end
            case 3: // volume
                r = color_triple_xfade(blue[0], pink[0], red[0], v);
                g = color_triple_xfade(blue[1], pink[1], red[1], v);
                b = color_triple_xfade(blue[2], pink[2], red[2], v);
                return;
            default: // attack, release: purple, brighter as it turns up
            {
                const float lvl = .2f + .8f * v;
                r = purple[0] * lvl; g = purple[1] * lvl; b = purple[2] * lvl;
                return;
            }
        }
    }

    /** CHORALE: knob 1 ring in chord mode, one hue per chord type */
    static inline void ChordColour(int idx, float &r, float &g, float &b)
    {
        const float h = float(idx) / float(kNumChords) * 6.f; // around the wheel
        const int   k = int(h);
        const float f = h - float(k);
        switch(k % 6)
        {
            case 0:  r = 1.f;     g = f;       b = 0.f;     break;
            case 1:  r = 1.f - f; g = 1.f;     b = 0.f;     break;
            case 2:  r = 0.f;     g = 1.f;     b = f;       break;
            case 3:  r = 0.f;     g = 1.f - f; b = 1.f;     break;
            case 4:  r = f;       g = 0.f;     b = 1.f;     break;
            default: r = 1.f;     g = 0.f;     b = 1.f - f; break;
        }
    }

    /** knob 1 moves one stacked note per detent (6 each way) */
    static const float kStackStep = 1.f / 10.f; // one interval per detent, 5 each way

    static const uint8_t knob_num_pages[6] = {2, 2, 2, 4, 1, 2}; // knob 4: TEMPO delay, mix, crush, doubler

    class NormalPage : public daisy::UiPage
    {
      private:
        /* SING: knobs 1-3, two pages each (press to switch):
         *    knob 1: stack     | strum
         *    knob 2: attack    | filter attack
         *    knob 3: release   | filter decay
         *  With the Chompi key (MenuPage): stack interval, onset, offset |
         *  -, filter cutoff, filter resonance. Doubler and spread are on
         *  knob 4, page 4. The defaults in ui.h, Harmonizer::Init and
         *  FilterEnv::Init must match. */
        void SingKnob(int knob, int page, float v)
        {
            switch(knob * 2 + page)
            {
                case 0: fx_->SetStack(v); break;
                case 1: fx_->SetStrum(v); break;
                /* CHORALE: knobs 2-3 turn the thresholds and the filter;
                   their envelopes are on the Chompi key (MenuPage) */
                case 2: fx_->SetGate(v); break;             // input threshold
                case 3: fx_->SetFilterEnvCutoff(v); break;  // filter cutoff
                case 4: fx_->SetFreeze(v); break;           // freeze threshold
                case 5: fx_->SetFilterEnvRes(v); break;     // filter resonance
                default: break;
            }
        }

        /** CHORALE: knob 2, page 1: the input threshold, live: magenta while
         *  the voice is over it, dim coral under it, dark when off */
        void InputThresholdColour(float v, float &r, float &g, float &b)
        {
            if (v <= 0.f)
                r = g = b = 0.f;
            else if (fx_->IsGateOpen())
            {
                r = sing_magenta[0]; g = sing_magenta[1]; b = sing_magenta[2];
            }
            else
            {
                const float lvl = .05f + .2f * v;
                r = sing_coral[0] * lvl; g = sing_coral[1] * lvl; b = sing_coral[2] * lvl;
            }
        }

        /** CHORALE: knob 3, page 1: the freeze threshold, live: white while
         *  recording, magenta while the voice is over it, dim gold under
         *  it, dark when off */
        void FreezeThresholdColour(float v, float &r, float &g, float &b)
        {
            if (v <= 0.f)
                r = g = b = 0.f;
            else if (fx_->IsCapturing())
                r = g = b = 1.f;
            else if (fx_->IsOverFreeze())
            {
                r = sing_magenta[0]; g = sing_magenta[1]; b = sing_magenta[2];
            }
            else
            {
                const float lvl = .05f + .2f * v;
                r = sing_gold[0] * lvl; g = sing_gold[1] * lvl; b = sing_gold[2] * lvl;
            }
        }

        /** white key 0..14 left to right for a MIDI note of the keyboard
         *  (48..72), or -1 for a black key */
        static int WhiteKeyIndex(int note)
        {
            static const int8_t kWhite[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
            const int n = note - 48;
            if (n < 0 || n > 24)
                return -1;
            const int w = kWhite[n % 12];
            return w < 0 ? -1 : w + 7 * (n / 12);
        }

        static void SingKnobColour(int knob, int page, float v, float &r, float &g, float &b)
        {
            float lvl = 1.f;
            /* one colour per control, brighter as it turns up */
            const float *c = white;
            switch(knob * 2 + page)
            {
                case 0: SingStackColour(v, r, g, b); return;
                case 1: // strum: warm white at centre, green up, dark orange down
                {
                    const float d = (v - .5f) * 2.f;
                    if (fabsf(d) < .02f)
                    {
                        r = sing_warm[0]; g = sing_warm[1]; b = sing_warm[2];
                        return;
                    }
                    c   = d < 0.f ? dark_orange : green;
                    lvl = .35f + .65f * fabsf(d);
                    break;
                }
                case 3: c = green; lvl = .15f + .85f * v; break;       // filter cutoff
                case 5: c = pale_yellow; lvl = .15f + .85f * v; break; // filter resonance
                default: break;
            }
            r = c[0] * lvl; g = c[1] * lvl; b = c[2] * lvl;
        }

      public:
    public:
        uint32_t init_time;
        bool init_ignore = true;

        void Init(Hardware *hw, Engine *fx, float** enc_arr, const float** def_arr, 
                    uint8_t* page, uint8_t midi_out_channel, bool ps_quant, bool split_delay)
        {
            hw_ = hw;
            fx_ = fx;
            enc_values = enc_arr;
            enc_defaults = def_arr;
            knob_page = page;

            midi_channel = midi_out_channel;

            quantized_pitch_ = !ps_quant;
            split_delay_ = split_delay;

            for (int knob = 0; knob < 6; knob++)
            {
                for (int page = 0; page < 4; page++)
                {
                    enc_values[page][knob] = enc_defaults[page][knob];
                }
            }

            if (split_delay_) {
                enc_values[0][3] = .5f;
            }

            
            fx_->SetInputGain(.75f);


            for (int i = 0; i < kNumSmtLeds; i++)
                SetSmtLed(i, 0, 0, 0);
            for (int i = 0; i < kNumPthLeds; i++)
                SetPthLed(i, 0, 0, 0);

            // 8mm leds
            SetPthLedFloat(0, 0.f, 0.f, 0.f);
            SetPthLedFloat(1, green[0], green[1], green[2]);
            SetPthLedFloat(2, 0.f, 0.f, 0.f);
            SetPthLedFloat(3, 0.f, 0.f, 0.f);
            SetPthLedFloat(4, 0.f, 0.f, 0.f);
            SetPthLedFloat(9, 0.f, 0.f, 0.f);

            // 5mm leds
            SetPthLedFloat(5, 1.f, 1.f, 1.f);
            SetPthLedFloat(6, 1.f, 1.f, 1.f);
            SetPthLedFloat(7, 1.f, 1.f, 1.f);
            SetPthLedFloat(8, 1.f, 1.f, 1.f);

            for (int i = 0; i < kNumSmtLeds; i++)
                SetSmtLed(i, 0, 0, 0);
            // SetSmtLedFloat(i, .1f, .1f, .0f);
            // SetSmtLedFloat(0, 0.f, 0.f, 0.f);
            // SetSmtLedFloat(9, 0.f, 0.f, 0.f);
            // SetSmtLedFloat(10, 0.f, 0.f, 0.f);

            init_time = System::GetNow();
        }

        uint32_t last_arm_blink;
        bool arm_blink = true;


        void ResetSmtLeds()
        {
            for(size_t i = 0; i < 25; i++)
            {
                SetSmtLed(i, 0.f, 0.f, 0.f);
            }
        }

        void CacheLeds()
        {
            std::copy(&led_pth_data[0][0], &led_pth_data[0][0] + kNumPthLeds * 3, &led_pth_cache[0][0]);
            std::copy(&led_smt_data[0][0], &led_smt_data[0][0] + kNumSmtLeds * 3, &led_smt_cache[0][0]);
        }

        void RefreshLeds()
        {
            std::copy(&led_pth_cache[0][0], &led_pth_cache[0][0] + kNumPthLeds * 3, &led_pth_data[0][0]);
            std::copy(&led_smt_cache[0][0], &led_smt_cache[0][0] + kNumSmtLeds * 3, &led_smt_data[0][0]);
        }

        void Draw(const daisy::UiCanvasDescriptor &canvasDescriptor) override
        {
            uint32_t now = System::GetNow();

            if(fx_->CheckReset())
            {
                enc_values[0][4] = enc_defaults[0][4];
                fx_->ResetLooperPitchQuant();
            }

            // ignore the first 1500 ms of inputs. Hack to stop random button presses on boot for now.
            if (init_ignore)
            {
                if (now - init_time > 1500)
                {
                    init_ignore = false;
                }
            }

            /* SING: held keys magenta, stacked notes gold; the middle C dim amber and
               the outer Cs dimmer, for orientation. Idle markers stay off
               while the mic is monitored, as in TAPE. */
            /* SING: for a moment after the stack knob turns, the white keys
               show one key per stacked note, from the middle C outwards */
            const uint32_t shown_age = System::GetNow() - shown_t_;
            const bool  show_value = shown_t_ != 0 && shown_age < kShowMs;
            const bool  chord_mode = fx_->IsChordMode();
            const int   tonic      = fx_->GetTonic();
            float       bar_lo = 0.f, bar_hi = 0.f, bar_lvl = 0.f;
            /* CHORALE: in chord mode, turning knob 1 previews the chord type
               on the tonic instead */
            float       preview[kMaxPoly];
            int         n_preview = 0;
            /* CHORALE: turning knob 1 previews what it does: the stack on
               a held middle C, or the chord type built on the root */
            if (show_value)
            {
                const float fade = shown_age < kShowMs - 400 ? 1.f
                                   : (kShowMs - shown_age) / 400.f;
                bar_lvl = .45f * fade;
                n_preview = chord_mode ? fx_->ChordNotes(float(tonic), preview)
                                       : fx_->StackPreview(0.f, preview);
            }

            /* SING: knob 6 held: the keys are an input meter, left to right,
               40 dB: green, then yellow from -12 dB, red from -4 dB */
            const bool show_meter = meter_held_ && now - meter_hold_ >= kMeterHoldMs;
            float meter = 0.f;
            if (show_meter)
            {
                const float vu = fx_->GetVUSample(VUTarget::VU_INPUT);
                meter = vu > .01f ? 1.f + 20.f * log10f(vu) / 40.f : 0.f;
            }

            for(size_t i = 7; i < (25 + 7); i++)
            {
                if (show_meter)
                {
                    const int pos = key_map[i] - 48; // 0..24, left to right
                    if (pos < meter * 25.f)
                    {
                        const float *c = pos >= 23 ? red : pos >= 17 ? yellow : green;
                        SetSmtLedFloat(led_map[i], c[0] * .6f, c[1] * .6f, c[2] * .6f);
                    }
                    else
                        SetSmtLed(led_map[i], 0, 0, 0);
                    continue;
                }
                /* CHORALE: slice mode: a sounding slice white, the white
                   keys dim yellow while the loop has slices */
                if (fx_->IsSliceMode())
                {
                    if (fx_->IsSlicePlaying(int(i)))
                        SetSmtLedFloat(led_map[i], 1.f, 1.f, 1.f);
                    else if (fx_->IsInSeq(int(i)) && (fx_->IsSeqLatched() || fx_->IsSeqPlaying()))
                        SetSmtLedFloat(led_map[i], yellow[0], yellow[1], yellow[2]);
                    else if (fx_->HasSlices() && WhiteKeyIndex(key_map[i]) >= 0)
                        SetSmtLedFloat(led_map[i], yellow[0] * .15f, yellow[1] * .15f, yellow[2] * .15f);
                    else
                        SetSmtLed(led_map[i], 0, 0, 0);
                    continue;
                }
                /* CHORALE: harmonies in a sequence: the sounding step white,
                   the other keys in it red (as TEMPO) */
                if ((fx_->IsSeqLatched() || fx_->IsSeqPlaying()) && fx_->IsInSeq(int(i)))
                {
                    if (fx_->SeqCurrentKey() == int(i))
                        SetSmtLedFloat(led_map[i], 1.f, 1.f, 1.f);
                    else
                        SetSmtLedFloat(led_map[i], .8f, 0.f, 0.f);
                    continue;
                }
                /* CHORALE: the tonic marks the keyboard, as the Cs did */
                const bool c_key = ((key_map[i] - 60 - tonic) % 12 + 12) % 12 == 0;
                const int  w     = WhiteKeyIndex(key_map[i]); // 0..14, -1 black
                if (fx_->IsHarmonyKeyHeld(i))
                    SetSmtLedFloat(led_map[i], sing_magenta[0], sing_magenta[1], sing_magenta[2]);
                else if (fx_->IsStackedNote(float(key_map[i]) - 60.f))
                    SetSmtLedFloat(led_map[i], sing_gold[0], sing_gold[1], sing_gold[2]);
                else if (show_value)
                {
                    bool on = false;
                    for (int k = 0; k < n_preview; k++)
                        on |= preview[k] == float(key_map[i]) - 60.f;
                    if (on)
                        SetSmtLedFloat(led_map[i], sing_gold[0] * bar_lvl, sing_gold[1] * bar_lvl, sing_gold[2] * bar_lvl);
                    else if (c_key) // the root stays marked under the preview
                    {
                        const float dim = key_map[i] == 60 + tonic ? .3f : .1f;
                        SetSmtLedFloat(led_map[i], sing_amber[0] * dim, sing_amber[1] * dim, sing_amber[2] * dim);
                    }
                    else
                        SetSmtLed(led_map[i], 0, 0, 0);
                }
                else if (c_key)
                {
                    const float dim = key_map[i] == 60 + tonic ? .3f : .1f;
                    SetSmtLedFloat(led_map[i], sing_amber[0] * dim, sing_amber[1] * dim, sing_amber[2] * dim);
                }
                else
                    SetSmtLed(led_map[i], 0, 0, 0);
            }

            // =========   encoders   =========
            for (int i = 0; i < 6; i++)
            {
                uint8_t page = knob_page[i];
                float value = enc_values[page][i];

                float r = 0.f; 
                float g = 0.f;
                float b = 0.f;
                switch (i)
                {
                case 0: // SING: knobs 1-3, see SingKnob()
                case 1:
                case 2:
                {
                    if (fx_->IsSliceMode())
                        SliceKnobColour(i, page, fx_->GetSliceKnob(page, i), r, g, b); // CHORALE
                    else if (page == 0 && i == 1)
                        InputThresholdColour(value, r, g, b);  // CHORALE: live
                    else if (page == 0 && i == 2)
                        FreezeThresholdColour(value, r, g, b); // CHORALE: live
                    else if (i == 0 && page == 0 && fx_->IsChordMode())
                        ChordColour(fx_->GetChordType(), r, g, b); // CHORALE
                    else
                        SingKnobColour(i, page, value, r, g, b);
                    SetPthLedFloat(i + 1, r, g, b);
                    break;
                }
                case 3: // magic
                {
                    if (page == 0) // CHORALE: TEMPO's delay / reverb, in its colours
                    {
                        fx_->SetGranularMain(value);
                        float c[3];
                        fx_->GetGranularColors(c);
                        r = c[0]; g = c[1]; b = c[2];
                    }
                    else if (page == 1) // CHORALE: TEMPO's delay mix
                    {
                        fx_->SetGranularMix(value);
                        r = color_triple_xfade(yellow[0], orange[0], red[0], value);
                        g = color_triple_xfade(yellow[1], orange[1], red[1], value);
                        b = color_triple_xfade(yellow[2], orange[2], red[2], value);
                    }
                    else if (page == 2) // SING: bitcrush (TAPE: saturation), red
                    {
                        fx_->SetCrush(value);
                        const float lvl = .15f + .85f * value;
                        r = red[0] * lvl; g = red[1] * lvl; b = red[2] * lvl;
                    }
                    else // SING: doubler (TAPE: filter), white
                    {
                        fx_->SetDoubler(value);
                        const float lvl = .15f + .85f * value;
                        r = g = b = lvl;
                    }


                    SetPthLedFloat(4, r, g, b);

                    break;
                }
                case 4: // CHORALE: the sequencer's steps on the wheel's lights
                {
                    if(fx_->IsSeqPlaying())
                    {
                        /* each step lights one side, alternately, fading
                           through the step */
                        const float lvl = 1.f - .85f * fx_->SeqPhase();
                        const int   on  = (fx_->SeqSteps() & 1) ? 6 : 5;
                        SetPthLedFloat(on, teal[0] * lvl, teal[1] * lvl, teal[2] * lvl);
                        SetPthLedFloat(on == 5 ? 6 : 5, 0.f, 0.f, 0.f);
                    }
                    else
                    {
                        const float lvl = fx_->IsSeqLatched() ? .25f : .04f;
                        SetPthLedFloat(5, teal[0] * lvl, teal[1] * lvl, teal[2] * lvl);
                        SetPthLedFloat(6, teal[0] * lvl, teal[1] * lvl, teal[2] * lvl);
                    }
                    break;
                }
                case 5: // gain
                {
                    if (page == 0)
                    {
                        float vu_sample = fx_->GetVUSample(VUTarget::VU_OUTPUT);

                        r = value * color_quad_xfade(.1f, green[0], yellow[0], pink[0], vu_sample);
                        g = value * color_quad_xfade(.1f, green[1], yellow[1], pink[1], vu_sample);
                        b = value * color_quad_xfade(.1f, green[2], yellow[2], pink[2], vu_sample);

                        fx_->SetMainGain(value);
                    }
                    else // SING: dry/wet, your voice warm white, harmonies magenta
                    {
                        SingMix(sing_warm, sing_magenta, value, r, g, b);
                    }

                    /* SING: both stay applied whichever page is shown; input
                       gain is set with the Chompi key on page 2 */
                    fx_->SetDryWet(enc_values[1][5]);
                    fx_->SetInputGain(enc_values[2][5]);
                    SetPthLedFloat(9, r, g, b);
                    break;
                }
                default:
                    break;
                }
            }

            /** PTH leds */
            float r, g, b;
            // CHORALE: play key: the sequencer, teal while it plays, dim
            // while latched
            {
                const float lvl = fx_->IsSeqPlaying() ? 1.f : fx_->IsSeqLatched() ? .2f : 0.f;
                SetPthLedFloat(led_map[33], teal[0] * lvl, teal[1] * lvl, teal[2] * lvl);
            }

            // loop key
            if(fx_->GetLooperIsEmpty() && !fx_->IsLooperRecordArmed())
            {
                r = g = b = 0.f;
            }
            else if(fx_->IsLooperRecordArmed())
            {
                if(now - last_arm_blink > 300)
                {
                    arm_blink = !arm_blink;
                    last_arm_blink = now;
                }

                r = arm_blink ? 1.f : 0.f;
                g = 0.f;
                b = 0.f;
            }
            else if(fx_->IsLooperFirstRecording() && fx_->IsLooperRecording())
            {
                r = red[0];
                g = red[1];
                b = red[2];
            }
            else if(fx_->IsLooperRecording()) // overdub
            {
                float position = fx_->GetLooperPosition();
                r = yellow[0] * position;
                g = yellow[1] * position;
                b = yellow[2] * position;
            }
            else
            {
                float position = fx_->GetLooperPosition();
                r = position;
                g = position;
                b = position;
            }

            SetPthLedFloat(led_map[34], r, g, b);

            // chompi key
            fx_->SetInputMonitor(true); // SING: dry voice per monitor mode (menu, knob 6)
            if (fx_->IsFrozen() || fx_->IsChordMode()) // CHORALE: chord mode; breathing while frozen
            {
                const float lvl = fx_->IsFrozen()
                    ? .55f + .45f * sinf(float(now % 1600) * (6.2831853f / 1600.f))
                    : 1.f;
                r = sing_magenta[0] * lvl;
                g = sing_magenta[1] * lvl;
                b = sing_magenta[2] * lvl;
            }
            else // input level, as TAPE shows while monitoring
            {
                float vu_sample = fx_->GetVUSample(VUTarget::VU_INPUT);
                r = color_quad_xfade(.1f, green[0], yellow[0], pink[0], vu_sample);
                g = color_quad_xfade(.1f, green[1], yellow[1], pink[1], vu_sample);
                b = color_quad_xfade(.1f, green[2], yellow[2], pink[2], vu_sample);
            }

            SetPthLedFloat(led_map[5], r, g, b);

            // ========   send the data   =========
            fill_led_data();
        }

        bool OnButton(uint16_t buttonID,
                      uint8_t numberOfPresses,
                      bool isRetriggering) override
        {
            if (init_ignore)
                return false;

            bool rising = numberOfPresses == 1;
            switch (buttonID)
            {
            // NO CONNECT, SKIP THESE
            case static_cast<uint16_t>(Hardware::SwId::NC_1): // fall through
            case static_cast<uint16_t>(Hardware::SwId::NC_2): // fall through
            case static_cast<uint16_t>(Hardware::SwId::NC_3): // fall through
            case static_cast<uint16_t>(Hardware::SwId::NC_4): // fall through
            case static_cast<uint16_t>(Hardware::SwId::NC_5): // fall through
                                                              // case static_cast<uint16_t>(Hardware::SwId::NC_6): // caught in ui.h
                break;

            // encoder clicks, toggle pages
            case static_cast<uint16_t>(Hardware::SwId::ENC_3_SW): // fall through
            case static_cast<uint16_t>(Hardware::SwId::ENC_1_SW): // fall through
            case static_cast<uint16_t>(Hardware::SwId::ENC_2_SW): // fall through
            case static_cast<uint16_t>(Hardware::SwId::ENC_4_SW): // fall through
            {
                if(!rising)
                {
                    uint8_t knob = key_map[buttonID];
                    knob_page[knob]++;
                    knob_page[knob] %= knob_num_pages[knob];
                }
                break;
            }

            case static_cast<uint16_t>(Hardware::SwId::ENC_6_SW):
            {
                /* SING: a short press turns the page, holding shows the
                   input meter on the keys instead */
                if(!rising && System::GetNow() - meter_hold_ < kMeterHoldMs)
                {
                    uint8_t knob = key_map[buttonID];
                    knob_page[knob]++;
                    knob_page[knob] %= knob_num_pages[knob];
                }

                meter_hold_ = System::GetNow();
                meter_held_ = rising;

                break;
            }

            // reset the looper pitch
            case ENC_5_SW:
            {
                /* CHORALE: a click toggles the sequencer's latch; holding it
                   and turning sets the rate instead */
                if (rising)
                {
                    wheel_held_   = true;
                    wheel_turned_ = false;
                }
                else
                {
                    wheel_held_ = false;
                    if (!wheel_turned_)
                        fx_->ToggleSeqLatch();
                }
                break;
            }

            // toggle. We're not using this anymore, just here in case something breaks
            case static_cast<uint16_t>(Hardware::SwId::SW_TOG): // fall through
                // switch_state = rising;
                // if (!rising)
                //     midi_channel = 0;

                // some weirdness results in handling this on edges rather than as pressed
                // for example if you hold the chompi key with the switch up then toggle the sw
                // down, you'll be on ch 1 until you release and repress the chompi key
                // then it will go to ch 2 like it should
                break;

            // CC buttons
            case static_cast<uint16_t>(Hardware::SwId::KEY_27): // play
            {
                /* CHORALE: play starts / stops the sequencer (the loop
                   itself: Chompi key + loop) */
                if(rising)
                    fx_->SetSeqPlay(!fx_->IsSeqPlaying());
                hw_->SendCC(midi_channel, 26, rising ? 127 : 0);
                break;
            }
            case static_cast<uint16_t>(Hardware::SwId::KEY_28): // loop
            {
                last_arm_blink = System::GetNow();
                fx_->LooperRecordButton(rising);
                hw_->SendCC(midi_channel, 27, rising ? 127 : 0);
                break;
            }

            case static_cast<uint16_t>(Hardware::SwId::KEY_26):
            {
                chompi_key_pressed = rising;
                hw_->SendCC(midi_channel, key_map[buttonID], rising ? 127 : 0);
 
                break;
            }

            // keys
            default:
                if (rising)
                {
                    // real keypress
                    if(!isRetriggering)
                    {
                        fx_->request_fifo.PushBack(KeyRequest(KeyRequest::Type::START, 
                            key_map[buttonID] - 60, buttonID, 127.f));
                    }

                    if(fx_->GetLooperRecordArm())
                        fx_->ToggleLooperRecord();

                    // this can take some time, so it must happen last
                    if(!isRetriggering)
                        hw_->SendNoteOn(midi_channel, key_map[buttonID], 127);
                }
                else
                {
                    if(!isRetriggering)
                    {
                        fx_->request_fifo.PushBack(KeyRequest(KeyRequest::Type::STOP, 
                            0, buttonID, 127.f));
                        hw_->SendNoteOff(midi_channel, key_map[buttonID], 127);
                    }
                }
                break;
            }

            return true;
        }

        bool OnEncoderTurned(uint16_t encoderID,
                             int16_t turns,
                             uint16_t stepsPerRevolution) override
        {
            if (init_ignore)
                return false;

            uint8_t page = knob_page[encoderID];
            float old_val = enc_values[page][encoderID];

            /* CHORALE: the wheel: tempo; held while turning, the rate
               (TEMPO's clock divisions) */
            if(encoderID == 4)
            {
                if(stepsPerRevolution > 0)
                    return true; // no CC for it
                if(wheel_held_)
                {
                    wheel_turned_ = true;
                    fx_->ChangeSeqRate(turns);
                }
                else
                    fx_->ChangeTempo(turns);
                return true;
            }

            /* CHORALE: slice mode: knobs 1-3 are TEMPO's slice controls
               (page 1 pitch, start, end, finely; page 2 volume, attack,
               release), kept apart from the harmony settings */
            if(encoderID <= 2 && fx_->IsSliceMode())
            {
                float v = fx_->GetSliceKnob(page, encoderID);
                if(stepsPerRevolution > 0)
                    v = turns / 127.f;
                else
                    v += turns * (page == 0 ? kEncoderFineStep : kEncoderCoarseStep);
                fx_->SetSliceKnob(page, encoderID, fclamp(v, 0.f, 1.f));
                return true;
            }

            /* CHORALE: in chord mode knob 1 (page 1) picks the chord type, one
               per detent, and previews it on the keys */
            if(page == 0 && encoderID == 0 && fx_->IsChordMode())
            {
                int idx = fx_->GetChordType();
                if(stepsPerRevolution > 0)
                    idx = turns * kNumChords / 128;
                else
                    idx += turns > 0 ? 1 : turns < 0 ? -1 : 0;
                fx_->SetChordType(idx);
                shown_t_ = System::GetNow();
                return true;
            }


            // overrode this to mean increment vs force knob position (used for CCs)
            if(stepsPerRevolution > 0)
            {
                enc_values[page][encoderID] = turns / 127.f;
            }
            else{
                float inc = turns * kEncoderCoarseStep;

                /* SING: stack (knob 1, page 1) moves a note per detent,
                   spread and doubler (knobs 2-3, page 1) in coarse ones */
                if(page == 0 && encoderID == 0)
                {
                    inc = turns * kStackStep;
                }
                else if(page == 0 && encoderID <= 2)
                {
                }
                else if(page == 1 && encoderID == 0)
                {
                    inc = turns * 3.f * kEncoderCoarseStep; // SING: strum, as fast as knobs 2-3
                }
                else if(page == 0 && encoderID == 3)
                {
                    inc = turns * kEncoderFineStep; // CHORALE: TEMPO's delay, finely
                }
                else if((encoderID == 0 && page == 0 && quantized_pitch_)
                    || (encoderID == 4 && quantized_pitch_))
                {
                    inc = 0.f;
                }
                else if ((encoderID == 0 && page == 0 && !quantized_pitch_)
                    || (encoderID == 1 && page == 0)
                    || (encoderID == 2 && page == 0)
                    || (encoderID == 4 && !quantized_pitch_))
                {
                    inc = turns * kEncoderFineStep;
                }

                enc_values[page][encoderID] += inc;
            }

            // clip
            enc_values[page][encoderID] = fclamp(enc_values[page][encoderID], 0.f, 1.f);

            /* SING: the stack knob sits exactly on a note count, so a CC
               position doesn't leave it between steps */
            if(page == 0 && encoderID == 0)
                enc_values[0][0] = .5f + Harmonizer<kMaxPoly>::StackPos(enc_values[0][0]) * kStackStep;

            if (encoderID <= 2)
            {
                SingKnob(encoderID, page, enc_values[page][encoderID]);
            }
            else if (encoderID == 4)
            {
                if (fx_->IsLooperPlaying())
                {
                    if(quantized_pitch_)
                        enc_values[0][4] = fx_->SetLooperPitchQuantized(turns, enc_values[0][4]);
                    else
                        fx_->SetLooperPitchFree(enc_values[0][4]);
                }
                else
                {
                    enc_values[0][4] = old_val;
                    fx_->SetLooperScrub(turns);
                }  
            }

            /* SING: the stack shows its notes on the white keys, briefly;
               the other knobs show themselves on their own LEDs */
            if(encoderID == 0 && page == 0)
                shown_t_ = System::GetNow();

            if (stepsPerRevolution == 0) {
                hw_->SendCC(midi_channel, cc_map[page][encoderID], enc_values[page][encoderID] * 127);
            }

            return true;
        }

        /** CHORALE: the toggle switch is slice mode. Compared with the engine
         *  on every call rather than on a change of the switch: the UI starts
         *  before the engine, whose Init() would otherwise undo the mode the
         *  switch was already in at power-on. */
        void SetSwitchState(bool state)
        {
            if (fx_->IsSliceMode() != state)
                fx_->SetSliceMode(state);
            switch_state = state; 
        }

        inline void SetInitIgnore(bool ignore) { init_ignore = ignore; }

    private:
        Hardware *hw_;
        Engine *fx_;

        float** enc_values;
        const float** enc_defaults;

        /** todo: these really shouldn't be stored in here
         *  Gonna move them out to a midi engine later
        */ 
        uint8_t midi_channel = 0;
        bool switch_state = false;

        /* SING: when the stack knob last turned, shown on the white keys */
        static constexpr uint32_t kShowMs = 1500;
        uint32_t shown_t_ = 0;
        bool chompi_key_pressed = false;
        uint8_t* knob_page;
        bool quantized_pitch_;
        bool split_delay_;

        /* CHORALE: the wheel held, and turned while held */
        bool wheel_held_ = false, wheel_turned_ = false;

        /* SING: knob 6 held: input meter on the keys */
        static constexpr uint32_t kMeterHoldMs = 500;
        bool     meter_held_ = false;
        uint32_t meter_hold_ = 0;
    };

} // namespace chompi