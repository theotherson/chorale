#include "hardware.h"
#include "DSPEngine.h"
#include "temp_led_stuff.h"

namespace chompi
{
    /** SING: knob 1 ring in the menu, one colour per stacked interval */
    static const float kIntervalColours[5][3] = {
        {1.f, .78f, .10f}, // 3rds: gold
        {1.f, .55f, 0.f},  // 4ths: amber
        {1.f, .42f, .30f}, // 5ths: coral
        {1.f, .45f, .60f}, // 6ths: rose
        {1.f, .85f, .65f}, // octaves: warm white
    };

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
            delay_time = .5f;
            resonance = 0.f;
            warble = 0.f;
            fx_->SetFinalComp(final_comp);
            fx_->SetFilterResonance(resonance);
            fx_->SetDelayTime(delay_time);
            fx_->SetWarble(warble);

            /* SING: what the Chompi key sets on knobs 2-4; the engine's
               Init (Harmonizer, FilterEnv) starts from the same values */
            onset_   = .4f;
            freeze_  = 0.f;
            glide_   = 0.f;
            cutoff_  = 1.f;
            env_res_ = 0.f;
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
        
            // play / overdub keys
            {
                const float gain = fx_->GetLooperDubGain();
                SetPthLedFloat(7, gain, gain, gain);
                SetPthLedFloat(8, gain, gain, gain);

                if(!fx_->GetLooperIsEmpty())
                {
                    float idx = enc_values[0][4] < .5f ? enc_values[0][4] * 2.f : (1.f - enc_values[0][4]) * 2.f; // 0 - 1 - 0
                    int led_on = enc_values[0][4] > .5f ? 6 : 5;
                    int led_off = enc_values[0][4] > .5f ? 5 : 6;

                    r = color_quad_xfade(med_blue[0], green[0], yellow[0], red[0], idx);
                    g = color_quad_xfade(med_blue[1], green[1], yellow[1], red[1], idx);
                    b = color_quad_xfade(med_blue[2], green[2], yellow[2], red[2], idx);

                    SetPthLedFloat(led_on, r, g, b);

                    if (idx > .8f)
                    {
                        float dim = (idx - .8f) * 5.f;

                        r = color_xfade(0.f, red[0], dim);
                        g = color_xfade(0.f, red[1], dim);
                        b = color_xfade(0.f, red[2], dim);

                        SetPthLedFloat(led_off, r, g, b);
                    }
                    else
                    {
                        SetPthLedFloat(led_off, 0.f, 0.f, 0.f);
                    }
                }
            }

            // shift encoder display
            if(chompi_key_pressed)
            {
                // FX
                if(fx_reset)
                {
                    SetPthLedFloat(4, 1.f, 1.f, 1.f);
                }
                else if(knob_page[3] == 0) // reverb page: delay time, blue
                {
                    const float lvl = .1f + .9f * delay_time;
                    SetPthLedFloat(4, blue[0] * lvl, blue[1] * lvl, blue[2] * lvl);
                }
                else if(knob_page[3] == 1) // SING: bitcrush page: compressor, peach
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

                // knob 1: the stacked interval (its key is lit too); page 2:
                // glide, teal, dark when off
                if(knob_page[0] == 0)
                {
                    const float *c = kIntervalColours[fx_->GetStackInterval()];
                    r = c[0]; g = c[1]; b = c[2];
                }
                else
                {
                    const float lvl = glide_ > 0.f ? .15f + .85f * glide_ : 0.f;
                    r = teal[0] * lvl; g = teal[1] * lvl; b = teal[2] * lvl;
                }
                SetPthLedFloat(1, r, g, b);

                /* knobs 2 and 3, each by its own page */
                if(knob_page[1] == 0)
                {
                    /* knob 2: input threshold, shown live: magenta while
                       the voice is over it, dim coral under it, dark off */
                    if(onset_ <= 0.f)
                        r = g = b = 0.f;
                    else if(fx_->IsGateOpen())
                    {
                        r = sing_magenta[0]; g = sing_magenta[1]; b = sing_magenta[2];
                    }
                    else
                    {
                        const float lvl = .05f + .2f * onset_;
                        r = sing_coral[0] * lvl; g = sing_coral[1] * lvl; b = sing_coral[2] * lvl;
                    }
                    SetPthLedFloat(2, r, g, b);
                }
                else // knob 2: filter cutoff, green
                {
                    const float lvl = fx_->IsFilterEnvOn() ? .15f + .85f * cutoff_ : 0.f;
                    SetPthLedFloat(2, green[0] * lvl, green[1] * lvl, green[2] * lvl);
                }

                if(knob_page[2] == 0)
                {
                    /* knob 3: freeze threshold, shown live: white while
                       recording, magenta while the voice is over it, dim
                       gold under it, dark with freeze off */
                    if(freeze_ <= 0.f || !fx_->IsFreezeOn())
                        r = g = b = 0.f;
                    else if(fx_->IsCapturing())
                        r = g = b = 1.f;
                    else if(fx_->IsOverFreeze())
                    {
                        r = sing_magenta[0]; g = sing_magenta[1]; b = sing_magenta[2];
                    }
                    else
                    {
                        const float lvl = .05f + .2f * freeze_;
                        r = sing_gold[0] * lvl; g = sing_gold[1] * lvl; b = sing_gold[2] * lvl;
                    }
                    SetPthLedFloat(3, r, g, b);
                }
                else // knob 3: filter resonance, pale yellow
                {
                    const float lvl = .15f + .85f * env_res_;
                    SetPthLedFloat(3, pale_yellow[0] * lvl, pale_yellow[1] * lvl, pale_yellow[2] * lvl);
                }

                /* SING: the on/off press shows for a moment: white on, dim off */
                if(flash_knob_ >= 0 && now - flash_t_ < kFlashMs)
                {
                    const float lvl = flash_on_ ? 1.f : .12f;
                    SetPthLedFloat(flash_knob_ + 1, lvl, lvl, lvl);
                }
            }

            // SING: no preset keys (TAPE: save / copy / erase)
            SetSmtLedFloat(7, 0.f, 0.f, 0.f);
            SetSmtLedFloat(8, 0.f, 0.f, 0.f);
            SetSmtLedFloat(9, 0.f, 0.f, 0.f);

            // FX pre / post looper
            int led_sel = fx_->GetFxPreLooper() ? 5 : 6;
            int led_off = fx_->GetFxPreLooper() ? 6 : 5;
            SetSmtLedFloat(led_sel, yellow[0], yellow[1], yellow[2]);
            SetSmtLedFloat(led_off, 0.f, 0.f, 0.f);
        
            // Input select
            led_sel = 2;
            led_sel += static_cast<int>(fx_->GetInputSource());
            SetSmtLedFloat(2, 0.f, 0.f, 0.f);
            SetSmtLedFloat(3, 0.f, 0.f, 0.f);
            SetSmtLedFloat(4, 0.f, 0.f, 0.f);
            SetSmtLedFloat(led_sel, pink[0], .7f * pink[1], .7f * pink[2]);

            // SING: no slots or banks on the keys
            for (uint8_t i = 1; i < 16; i++)
                SetSmtLedFloat(25 - i, 0.f, 0.f, 0.f);
            SetSmtLedFloat(0, 0.f, 0.f, 0.f);
            SetSmtLedFloat(1, 0.f, 0.f, 0.f);

            /* CHORALE: the scale on the keys, the tonic bright amber and the
               other scale notes dim; the keys with menu jobs (input, effects
               order: LEDs 2-6) keep their own lights */
            {
                const int tonic = fx_->GetTonic();
                for (size_t i = 7; i < (25 + 7); i++)
                {
                    const int led = led_map[i];
                    if (led >= 2 && led <= 6)
                        continue;
                    const int semis = int(key_map[i]) - 60;
                    if (((semis - tonic) % 12 + 12) % 12 == 0)
                        SetSmtLedFloat(led, sing_amber[0] * .7f, sing_amber[1] * .7f, sing_amber[2] * .7f);
                    else if (fx_->InScale(semis))
                        SetSmtLedFloat(led, sing_warm[0] * .12f, sing_warm[1] * .12f, sing_warm[2] * .12f);
                    else
                        SetSmtLedFloat(led, 0.f, 0.f, 0.f);
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

            /* CHORALE: knob 1 (page 1) picks the scale here, one per detent:
               major, minor, dorian, phrygian, lydian, mixolydian, harmonic
               minor, major and minor pentatonic; the keys show it */
            if(encoderID == 0 && page == 0)
            {
                const int step = turns > 0 ? 1 : turns < 0 ? -1 : 0;
                fx_->SetScale(fx_->GetScale() + step);
                return true;
            }

            /* SING: knobs 2-3 set the input gate (page 1: onset, offset)
               and the filter envelope's filter (page 2: cutoff, resonance) */
            if(encoderID == 1 || encoderID == 2)
            {
                float &val = page == 0 ? (encoderID == 1 ? onset_ : freeze_)
                                       : (encoderID == 1 ? cutoff_ : env_res_);
                val = fclamp(val + inc, 0.f, 1.f);
                if(page == 0 && encoderID == 1)      fx_->SetGate(val);
                else if(page == 0)                   fx_->SetFreeze(val);
                else if(encoderID == 1)              fx_->SetFilterEnvCutoff(val);
                else                                 fx_->SetFilterEnvRes(val);
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
                    if(page == 0) // magic
                    {
                        delay_time += inc;
                        delay_time = fclamp(delay_time, 0.f, 1.f);
                        fx_->SetDelayTime(delay_time);
                    }
                    else if (page == 1) // SING: bitcrush page: the compressor
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
                else if(encoderID == 4)
                {
                    if(quantized_pitch_)
                        enc_values[0][4] = fx_->SetLooperPitchQuantized(turns, enc_values[0][4]);
                    else
                    {
                        enc_values[0][4] += turns * kEncoderFineStep;
                        enc_values[0][4] = fclamp(enc_values[0][4], 0.f, 1.f);
                        fx_->SetLooperPitchFree(enc_values[0][4]);
                    }

                    float idx = enc_values[0][4] < .5f ? enc_values[0][4] * 2.f : (1.f - enc_values[0][4]) * 2.f; // 0 - 1 - 0
                    r = color_quad_xfade(med_blue[0], green[0], yellow[0], red[0], idx);
                    g = color_quad_xfade(med_blue[1], green[1], yellow[1], red[1], idx);
                    b = color_quad_xfade(med_blue[2], green[2], yellow[2], red[2], idx);
                    SetPthLedFloat(5, r, g, b);
                    SetPthLedFloat(6, r, g, b);
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
                if(rising) // CHORALE: next stack interval / SING: no strum
                {
                    if(knob_page[0] == 0)
                        fx_->SetStackInterval((fx_->GetStackInterval() + 1)
                                              % Harmonizer<kMaxPoly>::kNumIntervals);
                    else
                    {
                        enc_values[1][0] = enc_defaults[1][0];
                        fx_->SetStrum(enc_values[1][0]);
                    }
                }
                break;

            // reset the looper pitch via fall through
            case ENC_5_SW:
                return false;

            case static_cast<uint16_t>(Hardware::SwId::ENC_1_SW): // knob 2
                if(rising) // SING: filter envelope on/off
                {
                    fx_->ToggleFilterEnv();
                    flash_knob_ = 1;
                    flash_on_   = fx_->IsFilterEnvOn();
                    flash_t_    = System::GetNow();
                }
                break;

            case static_cast<uint16_t>(Hardware::SwId::ENC_2_SW): // knob 3
                if(rising) // SING: freeze on/off
                {
                    fx_->ToggleFreezeOn();
                    flash_knob_ = 2;
                    flash_on_   = fx_->IsFreezeOn();
                    flash_t_    = System::GetNow();
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
                    enc_values[0][3] = enc_defaults[0][3];
                    enc_values[1][3] = enc_defaults[1][3];
                    enc_values[2][3] = enc_defaults[2][3];

                    fx_->SetReverb(enc_values[0][3]);
                    fx_->SetDelayFeedback(enc_values[0][3]);
                    fx_->SetCrush(enc_values[1][3]);
                    fx_->SetDoubler(enc_values[2][3]); // SING: page 3 is the doubler

                    delay_time = .5f;
                    warble = 0.f;
                    final_comp = 0.f; // SING: the compressor is on knob 4 now

                    fx_->SetDelayTime(delay_time);
                    fx_->SetWarble(warble);
                    fx_->SetFinalComp(final_comp);

                    if (split_delay_) {
                        enc_values[0][3] = .5f;
                    }
                }
            }

            case static_cast<uint16_t>(Hardware::SwId::SW_TOG): // toggle (no longer used, here for safety)
                break;

            case static_cast<uint16_t>(Hardware::SwId::KEY_26): // chompi
                chompi_key_pressed = rising; // the menu closes on its release
                break;

            case static_cast<uint16_t>(Hardware::SwId::KEY_16): // TAPE: banks;
            case static_cast<uint16_t>(Hardware::SwId::KEY_17): // CHORALE: tonic
                if(rising)
                    fx_->SetTonic(int(key_map[buttonID]) - 60);
                break;

            case static_cast<uint16_t>(Hardware::SwId::KEY_18): // mic in, fall through
            case static_cast<uint16_t>(Hardware::SwId::KEY_19): // aux in, fall through
            case static_cast<uint16_t>(Hardware::SwId::KEY_20): // resample
            {
                if(rising)
                {
                    InputSource source;
                    if(buttonID == static_cast<uint16_t>(Hardware::SwId::KEY_18))
                        source = InputSource::MIC;
                    else if(buttonID == static_cast<uint16_t>(Hardware::SwId::KEY_19))
                        source = InputSource::LINE_IN;
                    else
                        source = InputSource::RESAMPLE;

                    fx_->SetInputSource(source);
                }
                else if(!rising)
                    return false; // note off falls through

                break;
            }


            case static_cast<uint16_t>(Hardware::SwId::KEY_21): // fx pre looper, fall through
            case static_cast<uint16_t>(Hardware::SwId::KEY_22): // fx post looper
            {
                if(rising)
                {
                    fx_->SetFxPreLooper(buttonID == static_cast<uint16_t>(Hardware::SwId::KEY_21));
                }
                else if(!rising)
                    return false; // note off falls through

                break;
            }


            case static_cast<uint16_t>(Hardware::SwId::KEY_23): // TAPE: erase,
            case static_cast<uint16_t>(Hardware::SwId::KEY_24): // copy,
            case static_cast<uint16_t>(Hardware::SwId::KEY_25): // save presets
                if(rising) // CHORALE: tonic
                    fx_->SetTonic(int(key_map[buttonID]) - 60);
                break;

            // white keys and play/pause
            default:
                // play pause, overdub gain setting
                if(buttonID == 33 || buttonID == 34)
                {
                    const float gain = buttonID == 33 ? -.1f : .1f;
                    fx_->IncrementLooperDubGain(gain);
                    break;
                }
                /* CHORALE: a key pressed in the menu sets the tonic; its
                   release falls through, so a key held from before the menu
                   still lets go */
                if(rising)
                {
                    fx_->SetTonic(int(key_map[buttonID]) - 60);
                    break;
                }
                return false;
            }

            return true;
        }

        void OnFocusGained() override
        {
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

        float delay_time, resonance, warble;
        float onset_, freeze_, cutoff_, env_res_, spread_, glide_; // SING

        /* SING: a press of knob 2 / 3 shows its ring white (on) or dim
           white (off) for kFlashMs */
        static constexpr uint32_t kFlashMs = 500;
        int      flash_knob_ = -1;
        bool     flash_on_   = false;
        uint32_t flash_t_    = 0;
        float final_comp;

        bool fx_reset = false;
        bool pitch_reset = false;

    };
} // namespace chompi
