#include "hardware.h"
#include "DSPEngine.h"
#include "temp_led_stuff.h"

namespace chompi
{

    class MenuPage : public daisy::UiPage
    {
    public:



        void Init(Hardware *hw, Engine *fx, float** enc_arr, const float** def_arr,
            uint8_t* page, bool ps_quant, bool split_delay)
        {
            hw_ = hw;
            fx_ = fx;
            enc_values = enc_arr;
            enc_defaults = def_arr;
            knob_page = page;
            
            quantized_pitch_ = ps_quant;
            split_delay_ = split_delay;


            last_blink = System::GetNow();

            chompi_key_pressed = false;


            input_toggled = false;

            final_comp = 0.f;
            grain_alt_ = 0.f;   // CHORALE: TEMPO's delay randomness and feedback
            grain_fb_  = .3f;
            resonance = 0.f;
            warble = 0.f;
            fx_->SetFinalComp(final_comp);
            fx_->SetFilterResonance(resonance);
            fx_->SetGranularAlt(grain_alt_);
            fx_->SetGranularFeedback(grain_fb_);
            fx_->SetWarble(warble);

            /* SING: what the Chompi key sets on knobs 2-4; the engine's
               Init (Harmonizer, FilterEnv) starts from the same values */
            /* CHORALE: the envelopes, on the Chompi key with knobs 2-3 */
            amp_attack_  = .1f;
            amp_release_ = .86f; // 1.5 s
            glide_   = 0.f;
            f_attack_    = 0.f;
            f_decay_     = .5f;
            spread_  = 0.f;
        }

        void Draw(const daisy::UiCanvasDescriptor &canvasDescriptor) override
        {
            /** PTH leds */
            float r, g, b;
            uint32_t now = System::GetNow();

            if(now - last_blink > 250)
            {
                blink_state = !blink_state;
                last_blink = now;
            }

            // chompi key
            if (chompi_key_pressed)
            {
                r = .67f;
                g = 0.f;
                b = 1.f;
            }
            else
            {
                r = g = b = 0.f;
            }
            SetPthLedFloat(0, r, g, b);
        
            /* CHORALE: Chompi key + loop held kLoopClearMs clears the loop */
            if(loop_held_ && now - loop_held_t_ > kLoopClearMs)
            {
                fx_->RequestLoopClear();
                loop_held_ = false;
            }

            /* CHORALE: play: the pattern mode (sequence, arp up, down, up
               and down, random: the interval colours); loop: the raw loop,
               white while it plays; the wheel's lights: the rest pattern
               (none dim, then brighter) */
            {
                const float *c = kIntervalColours[fx_->GetSeqMode()];
                SetPthLedFloat(7, c[0], c[1], c[2]);
                const float lp = fx_->IsLooperPlaying() ? 1.f : .05f;
                SetPthLedFloat(8, lp, lp, lp);
                const float rl = .08f + .23f * float(fx_->GetSeqRest());
                SetPthLedFloat(5, teal[0] * rl, teal[1] * rl, teal[2] * rl);
                SetPthLedFloat(6, teal[0] * rl, teal[1] * rl, teal[2] * rl);
            }

            // shift encoder display
            if(chompi_key_pressed)
            {
                // FX
                if(fx_reset)
                {
                    SetPthLedFloat(4, 1.f, 1.f, 1.f);
                }
                else if(knob_page[3] == 0) // CHORALE: TEMPO's delay: randomness, blue
                {
                    const float lvl = .1f + .9f * grain_alt_;
                    SetPthLedFloat(4, blue[0] * lvl, blue[1] * lvl, blue[2] * lvl);
                }
                else if(knob_page[3] == 1) // CHORALE: TEMPO's delay mix: feedback, orange
                {
                    const float lvl = .1f + .9f * grain_fb_;
                    SetPthLedFloat(4, 1.f * lvl, .3f * lvl, 0.f);
                }
                else if(knob_page[3] == 2) // SING: bitcrush page: compressor, peach
                {
                    const float lvl = .1f + .9f * final_comp;
                    SetPthLedFloat(4, orange[0] * lvl, orange[1] * lvl, orange[2] * lvl);
                }
                else // SING: doubler page: warble, white
                {
                    const float lvl = .1f + .9f * warble;
                    SetPthLedFloat(4, lvl, lvl, lvl);
                }

                if(input_toggled)
                {
                    switch(fx_->GetMonitorMode())
                    {
                        case MonitorMode::BOTH:
                            r = blue[0];
                            g = blue[1];
                            b = blue[2];
                            break;
                        case MonitorMode::HP:
                            r = orange[0];
                            g = orange[1];
                            b = orange[2];
                            break;
                        case MonitorMode::OFF: // SING: dry voice off
                            r = .15f;
                            g = b = 0.f;
                            break;
                        case MonitorMode::SEND_RET:
                        default:
                            r = yellow[0];
                            g = yellow[1];
                            b = yellow[2];
                            break;
                    }
                }
                else if(knob_page[5] == 1) // SING: input gain, blue to red
                {
                    r = color_xfade(blue[0], red[0], enc_values[2][5]);
                    g = color_xfade(blue[1], red[1], enc_values[2][5]);
                    b = color_xfade(blue[2], red[2], enc_values[2][5]);
                }
                else // SING: spread, white
                {
                    r = g = b = .1f + .9f * spread_;
                }

                SetPthLedFloat(9, r, g, b);

                // knob 1: notes mode, how many notes stack (gold, brighter
                // for more); chord mode, the voicing (one colour each);
                // page 2: glide, teal, dark when off
                if(knob_page[0] == 0 && fx_->IsChordMode())
                {
                    const float *c = kIntervalColours[fx_->GetVoicing()];
                    r = c[0]; g = c[1]; b = c[2];
                }
                else if(knob_page[0] == 0)
                {
                    const float lvl = .1f + .9f * float(fx_->GetStackNotes()) / 6.f;
                    r = sing_gold[0] * lvl; g = sing_gold[1] * lvl; b = sing_gold[2] * lvl;
                }
                else
                {
                    const float lvl = glide_ > 0.f ? .15f + .85f * glide_ : 0.f;
                    r = teal[0] * lvl; g = teal[1] * lvl; b = teal[2] * lvl;
                }
                SetPthLedFloat(1, r, g, b);

                /* knobs 2 and 3, each by its own page */
                if(knob_page[1] == 0 && fx_->IsSliceMode())
                {
                    /* slice mode: knob 2: how many slices, yellow, brighter
                       for more */
                    const float lvl = float(fx_->GetSliceCount()) / 16.f;
                    SetPthLedFloat(2, yellow[0] * lvl, yellow[1] * lvl, yellow[2] * lvl);
                }
                else // CHORALE: knob 2: attack yellow / filter attack purple
                {
                    const float *c   = knob_page[1] == 0 ? yellow : purple;
                    const float  v   = knob_page[1] == 0 ? amp_attack_ : f_attack_;
                    const float  lvl = .15f + .85f * v;
                    SetPthLedFloat(2, c[0] * lvl, c[1] * lvl, c[2] * lvl);
                }

                {   // CHORALE: knob 3: release deep orange (red when it never
                    // fades) / filter decay blue
                    const bool   inf = knob_page[2] == 0 && amp_release_ >= .99f;
                    const float *c   = inf ? red : knob_page[2] == 0 ? deep_orange : blue;
                    const float  v   = knob_page[2] == 0 ? amp_release_ : f_decay_;
                    const float  lvl = .15f + .85f * v;
                    SetPthLedFloat(3, c[0] * lvl, c[1] * lvl, c[2] * lvl);
                }

                /* SING: the on/off press shows for a moment: white on, dim off */
                if(flash_knob_ >= 0 && now - flash_t_ < kFlashMs)
                {
                    const float lvl = 1.f; // a reset flashes white
                    SetPthLedFloat(flash_knob_ + 1, lvl, lvl, lvl);
                }
            }

            /* CHORALE: the keys while the Chompi key is held.
               Upper octave (middle C to B): the root, light orange, the
               selected one pink. Lower white keys: the scale, green, the
               selected one bright. Lowest C# / D#: chord octave down / up,
               lit while shifted. F# below the middle C: chord mode (magenta
               when on). G#: the
               input (mic warm white, line teal, resample purple). A#:
               effects routing, blue, bright before the looper, dim after.
               The top C: unused. */
            {
                static const int kScaleKeys[7] = {48, 50, 52, 53, 55, 57, 59};
                const int tonic  = fx_->GetTonic();
                const int scale  = fx_->GetScale();
                const int octave = fx_->GetChordOctave();
                for (size_t i = 7; i < (25 + 7); i++)
                {
                    const int note = key_map[i];
                    const int led  = led_map[i];
                    float r = 0.f, g = 0.f, b = 0.f;
                    if (note >= 60 && note <= 71)
                    {
                        if (note - 60 == tonic) { r = pink[0]; g = pink[1]; b = pink[2]; }
                        else                    { r = .35f; g = .17f; b = .03f; } // light orange
                    }
                    else if (note == 49 || note == 51) // octave down / up
                    {
                        const int   shift = note == 49 ? -octave : octave;
                        const float lvl   = shift >= 2 ? 1.f : shift == 1 ? .5f : .05f;
                        r = g = b = lvl;
                    }
                    else if (note == 54) // chord mode: magenta, dim when off
                    {
                        const float lvl = fx_->IsChordMode() ? 1.f : .08f;
                        r = sing_magenta[0] * lvl; g = sing_magenta[1] * lvl; b = sing_magenta[2] * lvl;
                    }
                    else if (note == 56) // input
                    {
                        const int src = static_cast<int>(fx_->GetInputSource());
                        const float *c = src == 0 ? sing_warm : src == 1 ? teal : purple;
                        r = c[0] * .7f; g = c[1] * .7f; b = c[2] * .7f;
                    }
                    else if (note == 58) // effects routing
                    {
                        const float lvl = fx_->GetFxPreLooper() ? 1.f : .2f;
                        b = lvl; g = .25f * lvl;
                    }
                    else
                    {
                        for (int k = 0; k < 7; k++)
                            if (note == kScaleKeys[k])
                            {
                                const float lvl = k == scale ? 1.f : .12f;
                                g = lvl;
                            }
                    }
                    SetSmtLedFloat(led, r, g, b);
                }
            }

            // ========   send the data   =========
            fill_led_data();
        }


        bool OnEncoderTurned(uint16_t encoderID,
                             int16_t turns,
                             uint16_t stepsPerRevolution) override
        {
            uint8_t page = knob_page[encoderID];
            float inc = turns * kEncoderCoarseStep;

            // we're receiving a knob position via CC
            if(stepsPerRevolution > 0)
                return false; // fall through to normalpage

            /* CHORALE: knob 1 (page 1), one step per detent: in notes mode
               how many notes the stack adds (1-6), in chord mode the voicing
               (close, 1st, 2nd, 3rd inversion, open) */
            if(encoderID == 0 && page == 0)
            {
                const int step = turns > 0 ? 1 : turns < 0 ? -1 : 0;
                if(fx_->IsChordMode())
                    fx_->SetVoicing(fx_->GetVoicing() + step);
                else
                    fx_->SetStackNotes(fx_->GetStackNotes() + step);
                return true;
            }

            /* SING: knobs 2-3 set the input gate (page 1: onset, offset)
               and the filter envelope's filter (page 2: cutoff, resonance) */
            /* CHORALE: slice mode: knob 2 (page 1) sets how many slices,
               4, 8, 12 or 16, one step per detent */
            if(encoderID == 1 && page == 0 && fx_->IsSliceMode())
            {
                fx_->SetSliceCount(fx_->GetSliceCount() + (turns > 0 ? 4 : -4));
                return true;
            }

            if(encoderID == 1 || encoderID == 2)
            {
                /* CHORALE: the envelopes: page 1 the harmonies' attack and
                   release, page 2 the filter's attack and decay */
                float &val = page == 0 ? (encoderID == 1 ? amp_attack_ : amp_release_)
                                       : (encoderID == 1 ? f_attack_ : f_decay_);
                val = fclamp(val + inc, 0.f, 1.f);
                if(page == 0 && encoderID == 1)      fx_->SetAttack(val);
                else if(page == 0)                   fx_->SetDecay(val); // the release
                else if(encoderID == 1)              fx_->SetFilterEnvAttack(val);
                else                                 fx_->SetFilterEnvDecay(val);
                return true;
            }

            /* SING: knob 1 (strum page): glide */
            if(encoderID == 0 && page == 1)
            {
                glide_ = fclamp(glide_ + turns * 3.f * kEncoderCoarseStep, 0.f, 1.f);
                fx_->SetGlide(glide_);
                return true;
            }
            if(encoderID <= 2)
                return true;

            float r, g, b;
            {
                if (encoderID == 3)
                {
                    if(page == 0) // CHORALE: TEMPO's delay randomness
                    {
                        grain_alt_ = fclamp(grain_alt_ + inc, 0.f, 1.f);
                        fx_->SetGranularAlt(grain_alt_);
                    }
                    else if (page == 1) // CHORALE: TEMPO's delay feedback
                    {
                        grain_fb_ = fclamp(grain_fb_ + inc, 0.f, 1.f);
                        fx_->SetGranularFeedback(grain_fb_);
                    }
                    else if (page == 2) // SING: bitcrush page: the compressor
                    {
                        final_comp += inc;
                        final_comp = fclamp(final_comp, 0.f, 1.f);
                        fx_->SetFinalComp(final_comp);
                    }
                    else // SING: doubler page: warble
                    {
                        warble += inc;
                        warble = fclamp(warble, 0.f, 1.f);
                        fx_->SetWarble(warble);
                    }
                }
                else if(encoderID == 4) // CHORALE: the wheel: the rest pattern
                {
                    fx_->SeqStepRest(turns > 0 ? 1 : -1);
                }               
                else if(encoderID == 5)
                {
                    if(page == 1) // SING: input gain on the dry/wet page
                    {
                        enc_values[2][5] = fclamp(enc_values[2][5] + inc, 0.f, 1.f);
                        fx_->SetInputGain(enc_values[2][5]);
                    }
                    else // SING: spread (TAPE: the compressor)
                    {
                        spread_ = fclamp(spread_ + inc, 0.f, 1.f);
                        fx_->SetSpread(spread_);
                    }
                    input_toggled = false;
                }
            }


            return true;
        }

        bool OnButton(uint16_t buttonID,
                uint8_t numberOfPresses,
                bool isRetriggering) override
        {
            if(isRetriggering)
                return true;

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


            case static_cast<uint16_t>(Hardware::SwId::ENC_4_SW): // knob 1
                if(rising && fx_->IsSliceMode()) // CHORALE: slice mode: its own reset
                {
                    ResetSliceKnob(0);
                    break;
                }
                if(rising) // CHORALE: knob 1 back to its defaults (page 2: strum to the centre)
                {
                    if(knob_page[0] == 0)
                    {
                        enc_values[0][0] = enc_defaults[0][0];
                        fx_->SetStack(enc_values[0][0]);
                        fx_->SetStackNotes(1);
                        fx_->SetVoicing(0);
                        Flash(0);
                    }
                    else
                    {
                        enc_values[1][0] = enc_defaults[1][0];
                        fx_->SetStrum(enc_values[1][0]);
                    }
                }
                break;

            // reset the looper pitch via fall through
            case ENC_5_SW: // CHORALE: Chompi key + wheel click: tap tempo
                if(rising)
                    fx_->TapTempo();
                break;

            case static_cast<uint16_t>(Hardware::SwId::ENC_1_SW): // knob 2
                if(rising && fx_->IsSliceMode())
                    ResetSliceKnob(1);
                else if(rising) // CHORALE: everything on knob 2 back to its defaults
                {
                    enc_values[0][1] = enc_defaults[0][1]; // input threshold
                    enc_values[1][1] = enc_defaults[1][1]; // filter cutoff
                    fx_->SetGate(enc_values[0][1]);
                    fx_->SetFilterEnvCutoff(enc_values[1][1]);
                    amp_attack_ = .1f;
                    f_attack_   = 0.f;
                    fx_->SetAttack(amp_attack_);
                    fx_->SetFilterEnvAttack(f_attack_);
                    Flash(1);
                }
                break;

            case static_cast<uint16_t>(Hardware::SwId::ENC_2_SW): // knob 3
                if(rising && fx_->IsSliceMode())
                    ResetSliceKnob(2);
                else if(rising) // CHORALE: everything on knob 3 back to its defaults
                {
                    enc_values[0][2] = enc_defaults[0][2]; // freeze threshold
                    enc_values[1][2] = enc_defaults[1][2]; // filter resonance
                    fx_->SetFreeze(enc_values[0][2]);
                    fx_->SetFilterEnvRes(enc_values[1][2]);
                    amp_release_ = .86f; // 1.5 s
                    f_decay_     = .5f;
                    fx_->SetDecay(amp_release_);
                    fx_->SetFilterEnvDecay(f_decay_);
                    Flash(2);
                }
                break;

            case static_cast<uint16_t>(Hardware::SwId::ENC_6_SW): // volume
                if(rising)
                {
                    fx_->IncrementMonitorMode();
                    input_toggled = true;
                }
            break;

            case static_cast<uint16_t>(Hardware::SwId::ENC_3_SW): // magic wand
            {
                fx_reset = rising;
                if(rising)
                {
                    for (int pg = 0; pg < 4; pg++)
                        enc_values[pg][3] = enc_defaults[pg][3];

                    /* CHORALE: TEMPO's delay (off, mix centred, unfrozen),
                       bitcrush, doubler, compressor and warble */
                    fx_->SetGranularMain(enc_values[0][3]);
                    fx_->SetGranularMix(enc_values[1][3]);
                    fx_->SetGranularFreeze(false);
                    fx_->SetCrush(enc_values[2][3]);
                    fx_->SetDoubler(enc_values[3][3]);

                    grain_alt_ = 0.f;
                    grain_fb_  = .3f;
                    warble = 0.f;
                    final_comp = 0.f;

                    fx_->SetGranularAlt(grain_alt_);
                    fx_->SetGranularFeedback(grain_fb_);
                    fx_->SetWarble(warble);
                    fx_->SetFinalComp(final_comp);
                }
            }

            case static_cast<uint16_t>(Hardware::SwId::SW_TOG): // toggle (no longer used, here for safety)
                break;

            case static_cast<uint16_t>(Hardware::SwId::KEY_26): // chompi
                chompi_key_pressed = rising; // the menu closes on its release
                break;

            /* CHORALE: lowest C# / D#: chord octave down / up (both:
               back to 0); F# below the middle C: chord mode; G#: next input;
               A#: effects before / after the looper. Releases fall through. */
            case static_cast<uint16_t>(Hardware::SwId::KEY_16):
            case static_cast<uint16_t>(Hardware::SwId::KEY_17):
            {
                const bool down = buttonID == static_cast<uint16_t>(Hardware::SwId::KEY_16);
                (down ? oct_down_held_ : oct_up_held_) = rising;
                if(!rising)
                    return false;
                if(oct_down_held_ && oct_up_held_)
                    fx_->SetChordOctave(0);
                else
                    fx_->SetChordOctave(fx_->GetChordOctave() + (down ? -1 : 1));
                break;
            }

            case static_cast<uint16_t>(Hardware::SwId::KEY_18): // chord mode on / off
                if(!rising)
                    return false;
                fx_->SetChordMode(!fx_->IsChordMode());
                break;

            case static_cast<uint16_t>(Hardware::SwId::KEY_19): // input: mic, line, resample
                if(!rising)
                    return false;
                fx_->SetInputSource(InputSource((static_cast<int>(fx_->GetInputSource()) + 1) % 3));
                break;

            case static_cast<uint16_t>(Hardware::SwId::KEY_20): // effects before / after the looper
                if(!rising)
                    return false;
                fx_->SetFxPreLooper(!fx_->GetFxPreLooper());
                break;

            /* the upper black keys are roots, with the white ones below */
            case static_cast<uint16_t>(Hardware::SwId::KEY_21):
            case static_cast<uint16_t>(Hardware::SwId::KEY_22):
            case static_cast<uint16_t>(Hardware::SwId::KEY_23):
            case static_cast<uint16_t>(Hardware::SwId::KEY_24):
            case static_cast<uint16_t>(Hardware::SwId::KEY_25):
                if(!rising)
                    return false;
                fx_->SetTonic(int(key_map[buttonID]) - 60);
                break;

            // white keys and play/pause
            default:
                // play pause, overdub gain setting
                /* CHORALE: Chompi key + play: the next pattern mode; + loop:
                   the raw loop plays / stops */
                if(buttonID == 33)
                {
                    if(rising)
                        fx_->SeqNextMode();
                    break;
                }
                if(buttonID == 34) // tap: stop / start; held 1.5 s: clear (Draw)
                {
                    if(rising)
                    {
                        fx_->RequestLoopToggle();
                        loop_held_    = true;
                        loop_held_t_  = System::GetNow();
                    }
                    else
                        loop_held_ = false;
                    break;
                }
                /* CHORALE: a white key: upper octave the root, lower octave
                   the scale (major, minor, dorian, mixolydian, harmonic
                   minor, major and minor pentatonic), the top C nothing. Its
                   release falls through, so a key held from before the menu
                   still lets go */
                if(rising)
                {
                    static const int kScaleKeys[7] = {48, 50, 52, 53, 55, 57, 59};
                    const int note = key_map[buttonID];
                    if(note >= 60 && note <= 71)
                        fx_->SetTonic(note - 60);
                    for(int k = 0; k < 7; k++)
                        if(note == kScaleKeys[k])
                            fx_->SetScale(k);
                    break;
                }
                return false;
            }

            return true;
        }

        void OnFocusGained() override
        {
            loop_held_ = false; // CHORALE: a hold from an earlier menu doesn't count
            fx_reset = false;
            pitch_reset = false;
            chompi_key_pressed = true;

            if(quantized_pitch_)
            {
                fx_->ResetLooperPitchQuant();
            }

            input_toggled = false;

        }

        inline void SetSwitchState(bool state) { switch_state = state; }

        /** the menu stays open while the chompi key is held (SING: the toggle
         *  switch is the dry voice here, so it must not close the menu) */
        bool IsClosable()
        { 
            if(System::GetNow() - blink_startt > 1000 && !chompi_key_pressed)
            {
                if(!quantized_pitch_)
                {
                    fx_->ResetLooperPitchQuant();
                }
                return true;
            }
            return false;
        }

        bool switch_state = false;
        bool no_sd_card_ = false;
        inline void NoSDCard() { no_sd_card_ = true; }

    private:
        Hardware *hw_;
        Engine *fx_;
        float** enc_values;
        const float** enc_defaults;
        uint8_t* knob_page;

        bool quantized_pitch_;
        bool split_delay_;


        bool input_toggled;

        bool chompi_key_pressed = false;
        bool blink_state = true;
        uint32_t blink_startt;
        uint32_t last_blink;

        float grain_alt_, grain_fb_, resonance, warble; // CHORALE: TEMPO's delay controls
        float amp_attack_, amp_release_, f_attack_, f_decay_, spread_, glide_; // SING

        /* SING: a press of knob 2 / 3 shows its ring white (on) or dim
           white (off) for kFlashMs */
        static constexpr uint32_t kFlashMs = 500;
        /** CHORALE: slice mode: knob 1-3 back to its slice defaults */
        void ResetSliceKnob(int knob)
        {
            static const float kDefaults[2][3] = {{.5f, 0.f, 1.f}, {.6f, 0.f, .2f}};
            for (int pg = 0; pg < 2; pg++)
                fx_->SetSliceKnob(pg, knob, kDefaults[pg][knob]);
            Flash(knob);
        }

        /** a knob reset shows white on its ring for a moment */
        void Flash(int knob)
        {
            flash_knob_ = knob;
            flash_t_    = System::GetNow();
        }
        int      flash_knob_ = -1;
        bool     oct_down_held_ = false, oct_up_held_ = false; // CHORALE
        static constexpr uint32_t kLoopClearMs = 1500;     // CHORALE: Chompi + loop held
        bool     loop_held_   = false;
        uint32_t loop_held_t_ = 0;
        bool     flash_on_   = false;
        uint32_t flash_t_    = 0;
        float final_comp;

        bool fx_reset = false;
        bool pitch_reset = false;

    };
} // namespace chompi
