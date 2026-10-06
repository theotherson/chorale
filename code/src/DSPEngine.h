/** DSPEngine
 *  Core DSP for sampling engine, looping engine, and additional DSP
 */
#pragma once
#include "daisy.h"
#include "LooperEngine.h"
#include "daisysp.h"
#include "DJFilter.h"
#include "Warble.h"
#include "EnvFollower.h"
#include "MicFilter.h"
#include "Harmonizer.h"
#include "FilterEnv.h"
#include "SliceEngine.h" // CHORALE: TEMPO's vocal chops on the looper
#include "Sequencer.h"   // CHORALE: after TEMPO's arpeggiator / sequencer

/* SING: defined in chompi_main.cpp, in DTCM */
extern chompi::Harmonizer<7> harmonizer;
#include "reverb.h"
#include "RamBuffer.h"
#include "limiter.h"
#include "granularDelay.h"   // CHORALE: TEMPO's granular delay and reverb
#include "SimpleCompressor.h"
#include <algorithm>

using namespace daisy;

static constexpr float kLineOutGain = .3f;
static constexpr float kHpGain = .2f;
static constexpr float kMicGain = 5.f;
static constexpr float kLineInGain = 3.f;
static constexpr size_t kMaxPoly = 7;

namespace daisy
{
    enum class VUTarget
    {
        VU_INPUT = 0,
        VU_OUTPUT,
        VU_LAST
    };

    enum class InputSource
    {
        MIC = 0,
        LINE_IN,
        RESAMPLE,
        LAST,
    };

    enum class MonitorMode
    {
        HP = 0,
        BOTH,
        SEND_RET, // TAPE's; SING's menu does not offer it
        OFF,      // SING: no dry voice
        LAST,
    };

    struct KeyRequest
    {
        enum class Type
        {
            START,
            STOP,
            DUMMY,
        };

        Type type_;
        float transpose_nn_;
        int key_;
        float vel_;

        /** constructor for full request data */
        KeyRequest(Type type,
                    float transpose_nn,
                    int key,
                    float vel
                  )
            : type_(type),
                transpose_nn_(transpose_nn),
                key_(key),
                vel_(vel)
        {
        }

        /** Empty, invalid request */
        KeyRequest()
            : type_(Type::DUMMY),
                transpose_nn_(0.f),
                key_(0),
                vel_(127.f)
        {
        }


    };

    /** @brief core engine for running entire modules audio
     *
     *  For now now abstraction for various sampling modes,
     *  possibly that can all be handled in the UI.
     *
     *  This will build out CHOMPI Mode and add the looping engine, and other fixed engines
     */
    class Engine
    {
    public:
        Engine() {}
        ~Engine() {}

        void Init(
            float samplerate, 
            daisysp::Reverb* reverb, 
            granularDelay* gdelay,
            RamBufferMemory* loop_buff,
            bool tape_slew,
            MonitorMode mon_mode)
        {
            monitor_mode = mon_mode;

            input_env_follower.Init();
            output_env_follower.Init();


            reverb_ = reverb;

            reverb_->Init(samplerate);
            reverb_->SetAmount(0.f);
            reverb_->SetInputGain(.3f);
            reverb_->SetLowpass(1.f);

            /* CHORALE: TEMPO's granular delay, its reverb on the delay's
               output, and its compressor ducking the delay under the dry */
            gdelay_ = gdelay;
            gcomp_.Init();
            gwet_ = gwet_target_ = gdry_ = gdry_target_ = 1.f;
            reverb_amt_ = reverb_amt_target_ = 0.f;
            reverb_boost_ = reverb_boost_target_ = 0.f;
            SetGranularMain(.5f);
            SetGranularMix(.5f);
            SetGranularAlt(0.f);
            SetGranularFeedback(.3f);

            mic_filter_.Init(samplerate);
            harmonizer.Init(samplerate);
            filter_env_.Init(samplerate);

            filter_.Init(samplerate);
            filter_.SetControl(.5f);
            cutoff_target_ = .5f;
            res_ = res_target_ = 0.f;

            SetCrush(0.f);
            crush_ = 0.f;
            crush_phase_ = 1.f;
            crush_l_ = crush_r_ = 0.f;

            mgain_ = mgain_target_ = .8f;
            dry_ = dry_target_ = .5f; wet_ = wet_target_ = 1.f; // CHORALE: 75% wet

            warble_.Init(samplerate);
            warble_.SetFreq(.1f);
            
            dcblock_mic_in_.Init(samplerate);
            dcblock_line_in_l_.Init(samplerate);
            dcblock_line_in_r_.Init(samplerate);
            dcblock_fx_l_.Init(samplerate);
            dcblock_fx_r_.Init(samplerate);


            fx_pre_loop = true;
            fx_env_ = fx_env_target_ = 1.f;
            final_lim_ = 0.f;

            resamp_env_ = resamp_env_target_ = 1.f;


            /* looper */
            looper.Init(samplerate, loop_buff, tape_slew);
            loop_mem_ = loop_buff;
            slices_.Init(samplerate);
            slice_mode_ = false;
            seq_.Init();
            seq_key_ = -1;
            seq_since_ = 0;
            seq_period_ = 9000;
            seq_steps_ = 0;

            /** Final output compressors */
            lim_hp_l_.Init();
            lim_hp_r_.Init();
            lim_line_l_.Init();
            lim_line_r_.Init();
        }

        // fill chompi buffer with 2 second long cosine
        void ApplyFx(float* outl, float* outr, size_t size)
        {
            /* SING: crush (knob 4, page 2) replaces TAPE's saturation.
               Bits and sample rate drop together: 16 -> 6 bits and
               48 kHz -> 3 kHz (lower cut quiet singing out entirely).
               Set once per block, slewed over blocks. */
            fonepole(crush_, crush_target_, .05f);
            const bool  crushing = crush_ > .001f;
            const float levels   = powf(2.f, 16.f - 10.f * crush_) * .5f; // steps per unit
            const float rate_inc = 1.f / powf(16.f, crush_);              // new sample every 1/inc

            for(size_t i = 0; i < size; i++)
            {
                // input gain
                fonepole(fx_env_, fx_env_target_, .001f);
                outl[i] *= fx_env_;
                outr[i] *= fx_env_;

                outl[i] = dcblock_fx_l_.Process(outl[i]);
                outr[i] = dcblock_fx_r_.Process(outr[i]);

                // SING: no DJ filter here (TAPE's knob 4 page 3); the filter
                // envelope on knobs 2-3 filters the live sound instead

                // SING: crush, sample-and-hold then fewer bits
                if(crushing)
                {
                    crush_phase_ += rate_inc;
                    if(crush_phase_ >= 1.f)
                    {
                        crush_phase_ -= 1.f;
                        crush_l_ = roundf(outl[i] * levels) / levels;
                        crush_r_ = roundf(outr[i] * levels) / levels;
                    }
                    outl[i] = crush_l_;
                    outr[i] = crush_r_;
                }

                // then clip
                outl[i] = daisysp::SoftClip(outl[i]);
                outr[i] = daisysp::SoftClip(outr[i]);

                // wow and flutter
                warble_.Process(outl[i], outr[i], &outl[i], &outr[i]);
            }

            /* CHORALE: TEMPO's delay and reverb (FxEngine::Process). The
               send goes into the granular delay; its output, plus some of
               the send (reverb boost), into the reverb; the compressor ducks
               that under the dry signal */
            for(size_t i = 0; i < size; i++)
            {
                fonepole(gwet_, gwet_target_, .001f);
                fonepole(gdry_, gdry_target_, .001f);
                fonepole(reverb_amt_, reverb_amt_target_, .001f);
                fonepole(reverb_boost_, reverb_boost_target_, .001f);

                const float wet_l = outl[i] * gwet_, wet_r = outr[i] * gwet_;
                float       dry_l = outl[i] * gdry_, dry_r = outr[i] * gdry_;

                float dl = 0.f, dr = 0.f;
                gdelay_->write(wet_l, wet_r);
                gdelay_->read(&dl, &dr);

                reverb_->SetAmount(reverb_amt_ * reverb_amt_ * .8f);
                reverb_->SetTime(reverb_amt_);
                reverb_->SetLowpass(reverb_amt_ * .55f + .4f);
                reverb_->SetDiffusion(reverb_amt_ * .6f);

                dl += wet_l * reverb_boost_;
                dr += wet_r * reverb_boost_;
                reverb_->Process(&dl, &dr);

                gcomp_.Process(&dl, &dr, &dry_l, &dry_r);

                outl[i] = dl + dry_l;
                outr[i] = dr + dry_r;
            }
        }

        /** Apply the total monitor signal to the input envelope follower */
        void ApplyEnvelopeFollower(size_t size, float* monitor)
        {
            for (size_t i = 0; i < size; i++)
                input_env_follower.Process((monitor[i] + monitor[size + i]) * .8f);
        }

        /** add the mic monitor into both outputs, 
            also add it to the monitor buffer, which will be used to feed the env follower at the end
        */
        void ApplyMicMonitor(const float* const* in, float **out, size_t size, float* monitor)
        {
            for (size_t i = 0; i < size; i++)
            {
                const float m = mic_d_ ? mic_d_[i] : in[0][i]; // SING: pre-delayed
                float sig = dcblock_mic_in_.Process(m * ingain_ * kMicGain
                                                    * (duck_ ? duck_[i] : 1.f));
                sig = mic_filter_.Process(sig);

                monitor[i] += sig;
                monitor[size + i] += sig;

                /* SING: dry/wet, then the filter envelope */
                const float d = filter_env_.Process(FilterEnv::DRY_L, i,
                                                    sig * (dry_buf_ ? dry_buf_[i] : 1.f));
                out[0][i] += d;
                out[1][i] += d;
            }
        }

        /** add the line in monitor into both outputs, 
            also add it to the monitor buffer, which will be used to feed the env follower at the end
        */
        void ApplyLineMonitor(const float* const* in, float **out, size_t size, float* monitor)
        {
            for (size_t i = 0; i < size; i++)
            {
                const float sigl = dcblock_line_in_l_.Process(in[2][i] * ingain_ * kLineInGain);
                const float sigr = dcblock_line_in_r_.Process(in[3][i] * ingain_ * kLineInGain);

                monitor[i] += sigl;
                monitor[size + i] += sigr;

                /* SING: dry/wet, then the filter envelope */
                out[0][i] += filter_env_.Process(FilterEnv::DRY_L, i, sigl * (dry_buf_ ? dry_buf_[i] : 1.f));
                out[1][i] += filter_env_.Process(FilterEnv::DRY_R, i, sigr * (dry_buf_ ? dry_buf_[i] : 1.f));
            }
        }

        bool looper_reset = false;
        bool CheckReset()
        {
            if(looper_reset)
            {
                looper_reset = false;
                return true;
            }

            return false;
        }

        float old_fx_outl, old_fx_outr;
        void Process(const float *const *in, float **out, size_t size)
        {            
            // we'll use out[0] and out[1] as our working space, 
            // then copy to out[2] and out[3] at the end (and apply gain settings)
            std::fill(out[0], out[0] + size, 0.f);
            std::fill(out[1], out[1] + size, 0.f);

            /* SING: key-click ducking for the built-in mic, used
               by the harmonizer input and by the dry mic monitor below */
            float duck[size];
            harmonizer.Duck(duck, size);
            duck_ = duck;

            /* SING: the built-in mic, kMicPreDelay late, for the harmonies and
               the dry voice: the duck starts on a button's first contact,
               about a block after its click does, so this lets it cover the
               click from its first sample (after ugrossek's sing-pitch SING) */
            float mic_d[size];
            for(size_t i = 0; i < size; i++)
            {
                mic_delay_[mic_delay_w_] = in[0][i];
                mic_d[i] = mic_delay_[(mic_delay_w_ - kMicPreDelay) & (kMicDelayLen - 1)];
                mic_delay_w_ = (mic_delay_w_ + 1) & (kMicDelayLen - 1);
            }
            mic_d_ = mic_d;

            /* SING: dry/wet (knob 6, page 2), smoothed per sample: the dry
               gain goes to the voice monitor below, the wet to the harmonies */
            float dry[size], wet[size];
            for(size_t i = 0; i < size; i++)
            {
                fonepole(dry_, dry_target_, .001f);
                fonepole(wet_, wet_target_, .001f);
                dry[i] = dry_;
                wet[i] = wet_;
            }
            dry_buf_ = dry;

            /* SING: harmony voices from the live input, or in resample mode
               from the loop (as it played in the last block); FX and looper
               follow as they did for TAPE's sample voices */
            const bool resample = in_source == InputSource::RESAMPLE;
            if(harmonizer.Active()
               && (in_source == InputSource::MIC || in_source == InputSource::LINE_IN
                   || resample))
            {
                float live[size];
                const bool mic = in_source == InputSource::MIC;
                for(size_t i = 0; i < size; i++)
                {
                    if(resample)
                        live[i] = i < kLoopBufLen ? loop_buf_[i] * ingain_ * kLoopGain : 0.f;
                    else
                        live[i] = mic ? mic_d[i] * ingain_ * kMicGain * duck[i]
                                      : (in[2][i] + in[3][i]) * .5f * ingain_ * kLineInGain;
                }
                harmonizer.Process(live, mic, out[0], out[1], size);
                if(harmonizer.TakeEntered())
                    filter_env_.Trigger();
            }

            /* SING: the filter envelope moves once per block; the
               harmonies (here) and the dry voice (in the monitors) use it */
            filter_env_.UpdateBlock(size);
            for(size_t i = 0; i < size; i++)
            {
                out[0][i] = filter_env_.Process(FilterEnv::HARM_L, i, out[0][i] * wet[i]);
                out[1][i] = filter_env_.Process(FilterEnv::HARM_R, i, out[1][i] * wet[i]);
            }

            /* CHORALE: a sequenced note lets go after kSeqGate of its step */
            seq_since_ += uint32_t(size);
            if (seq_key_ >= 0 && float(seq_since_) > kSeqGate * float(seq_period_))
            {
                TargetOff(seq_key_);
                seq_key_ = -1;
            }

            /* CHORALE: the slices of the loop, as it is now */
            slices_.SetSource(loop_mem_->mem, loop_mem_->length / 2);
            slices_.Process(out[0], out[1], size);

            for(size_t i = 0; i < size; i++)
            {
                out[0][i] = daisysp::SoftClip(out[0][i]);
                out[1][i] = daisysp::SoftClip(out[1][i]);
            }

            // add the monitor to the working space pre-FX
            float monitor[2][size];
            std::fill(&monitor[0][0], &monitor[1][size], 0.f);

            if(monitor_mode == MonitorMode::BOTH)
            {
                if(in_source == InputSource::MIC)
                    ApplyMicMonitor(in, out, size, &monitor[0][0]);
                else if(in_source == InputSource::LINE_IN)
                    ApplyLineMonitor(in, out, size, &monitor[0][0]);
            }
            else if (monitor_mode == MonitorMode::SEND_RET && input_monitor && in_source == InputSource::MIC)
            {
                ApplyMicMonitor(in, out, size, &monitor[0][0]);
            }


            /* CHORALE: the loop's stop / start and clear, asked for from the
               menu (Chompi key + loop), done here with the looper */
            if(loop_toggle_req_)
            {
                loop_toggle_req_ = false;
                if(!looper.IsFirstRecording())
                    looper.TogglePlaying();
            }
            if(loop_clear_req_)
            {
                loop_clear_req_ = false;
                looper.Reset();
                looper_reset = true;
            }

            /** looper read + write */
            looper.CheckRecordReady();
            if(looper.CheckReset())
                looper_reset = true;

            //fx_xfade_pre
            if(fx_pre_loop)
                ApplyFx(out[0], out[1], size);

            /* SING: the loop's own playback is what the looper adds; keep it
               for resample mode's harmonies, and there let dry/wet set its
               level (dry = the loop, wet = its harmonies) */
            float pre_l[size], pre_r[size];
            std::copy(out[0], out[0] + size, pre_l);
            std::copy(out[1], out[1] + size, pre_r);

            looper.Process(out[0], out[1], size);

            for(size_t i = 0; i < size; i++)
            {
                const float ll = out[0][i] - pre_l[i];
                const float lr = out[1][i] - pre_r[i];
                if(i < kLoopBufLen)
                    loop_buf_[i] = (ll + lr) * .5f;
                if(resample)
                {
                    out[0][i] = pre_l[i] + ll * dry[i];
                    out[1][i] = pre_r[i] + lr * dry[i];
                }
            }

            if(!fx_pre_loop)
                ApplyFx(out[0], out[1], size);

            if(fx_env_ < .01f)
            {
                fx_pre_loop = !fx_pre_loop;
                fx_env_target_ = 1.f;
            }

            //working space (HP) copy to line out
            std::copy(out[0], out[0] + size, out[2]);
            std::copy(out[1], out[1] + size, out[3]);

            // add the dry monitor to the HPs only
            if (monitor_mode == MonitorMode::HP && input_monitor)
            {
                if (in_source == InputSource::MIC)
                    ApplyMicMonitor(in, out, size, &monitor[0][0]);
                if (in_source == InputSource::LINE_IN)
                    ApplyLineMonitor(in, out, size, &monitor[0][0]);
            }
            else if (monitor_mode == MonitorMode::SEND_RET)
            {
                ApplyLineMonitor(in, out, size, &monitor[0][0]);
            }

            // apply resample gain as appropriate (and envelope in/out of that situation)
            if(in_source == InputSource::RESAMPLE)
            {
                for(size_t i = 0; i < size; i++)
                {
                    daisysp::fonepole(resamp_env_, resamp_env_target_, .001f);

                    out[0][i] = monitor[0][i] = resamp_env_ * out[0][i];
                    out[1][i] = monitor[1][i] = resamp_env_ * out[1][i];
        
                    input_env_follower.Process((monitor[0][i] + monitor[1][i]) * .2f);

                    if(monitor_mode == MonitorMode::BOTH)
                    {
                        out[2][i] = out[0][i];
                        out[3][i] = out[1][i];
                    }
                }
            }
            else
            {
                ApplyEnvelopeFollower(size, &monitor[0][0]);
            }


            /** main gain control */
            for(size_t i = 0; i < size; i++)
            {
                fonepole(mgain_, mgain_target_, .001f);
                fonepole(ingain_, ingain_target_, .001f);
                fonepole(final_lim_, final_lim_target_, .001f);

                // apply headphone and main gain
                out[0][i] *= kHpGain * mgain_;
                out[1][i] *= kHpGain * mgain_;

                // apply lineout and main gain
                out[2][i] *= kLineOutGain * mgain_;
                out[3][i] *= kLineOutGain * mgain_;

                // compress
                const float thresh = 1.f / (10.f * final_lim_ + 4.f);
                const float ratio = 1.f + final_lim_ * final_lim_ * 7.f;
                const float makeup = .9f + final_lim_ * .6f;
                const float pregain = 7.f * final_lim_ + 1.f;
                out[0][i] = lim_hp_l_.ProcessComp(out[0][i], pregain, thresh, ratio, makeup);
                out[1][i] = lim_hp_r_.ProcessComp(out[1][i], pregain, thresh, ratio, makeup);
                out[2][i] = lim_line_l_.ProcessComp(out[2][i], pregain, thresh, ratio, makeup);
                out[3][i] = lim_line_r_.ProcessComp(out[3][i], pregain, thresh, ratio, makeup);

                output_env_follower.Process((out[0][i] + out[1][i]));
            }

            duck_    = nullptr; // it pointed into this block's stack
            mic_d_   = nullptr; // so did this
            dry_buf_ = nullptr; // so did this
        }

        inline void SetInputMonitor(bool monitor) 
        {
            input_monitor = monitor;
            
            if(in_source == InputSource::RESAMPLE)
                resamp_env_target_ = monitor ? ingain_target_ : 1.f;
        }

        inline float GetVUSample(VUTarget target)
        { 
            if(target == VUTarget::VU_INPUT)
                return input_env_follower.GetLastSamp();
            else if(target == VUTarget::VU_OUTPUT)
                return output_env_follower.GetLastSamp();

            return 0.f;
        }


        void ProcessKeyReqs()
        {
            if (!request_fifo.IsEmpty())
            {
                KeyRequest req = request_fifo.PopFront();
                
                /* SING: keys play harmony voices of the live
                   input instead of samples. transpose_nn_ is the key's
                   distance from the middle C in semitones. */
                /* CHORALE: the sequencer keeps every key; while it plays,
                   it decides what sounds (SeqStep), else the key plays
                   at once, as a slice or a harmony */
                if (req.type_ == KeyRequest::Type::START)
                {
                    seq_.KeyDown(req.key_, req.transpose_nn_);
                    if (!seq_.Playing())
                        TargetOn(req.key_, req.transpose_nn_);
                }
                else if (req.type_ == KeyRequest::Type::STOP)
                {
                    seq_.KeyUp(req.key_);
                    if (!seq_.Playing())
                        TargetOff(req.key_);
                }
            }
            harmonizer.Refresh(); // SING: re-voice the stack after key or knob changes
        }

        void Prepare()
        {
            ProcessKeyReqs();
        }

        /* SING: knobs 1-3 drive the harmonizer */
        void SetStack(float val) { harmonizer.SetStack(val); }
        void SetStrum(float val) { harmonizer.SetStrum(val); }
        /* SING: a button contact changed (raw, before debouncing) */
        void KeyContact() { harmonizer.KeyContact(); }
        void SetGlide(float val) { harmonizer.SetGlide(val); }
        void SetFreeze(float val) { harmonizer.SetFreeze(val); }
        void ToggleFreezeOn() { harmonizer.SetFreezeOn(!harmonizer.FreezeOn()); }
        bool IsFreezeOn() { return harmonizer.FreezeOn(); }
        void ToggleFilterEnv() { filter_env_.SetEnabled(!filter_env_.Enabled()); }
        bool IsFilterEnvOn() { return filter_env_.Enabled(); }
        float GetFreeze() { return harmonizer.Freeze(); }
        bool IsOverFreeze() { return harmonizer.OverFreeze(); }
        bool IsCapturing() { return harmonizer.Capturing(); }
        void SetFilterEnvAttack(float val) { filter_env_.SetAttack(val); }
        void SetFilterEnvDecay(float val) { filter_env_.SetDecay(val); }
        void SetFilterEnvCutoff(float val) { filter_env_.SetCutoff(val); }
        void SetFilterEnvRes(float val) { filter_env_.SetRes(val); }
        void SetGate(float val) { harmonizer.SetGate(val); }
        float GetGate() { return harmonizer.Gate(); }
        bool IsGateOpen() { return harmonizer.GateOpen(); }
        void SetStackInterval(int idx) { harmonizer.SetInterval(idx); }
        void SetStackNotes(int n) { harmonizer.SetStackNotes(n); }

        /* CHORALE: slice mode (Chompi key + F# below the middle C): the keys
           play the loop's 16 slices instead of harmonies */
        void SetSliceMode(bool on)
        {
            harmonizer.AllOff();
            slices_.AllOff();
            seq_.Clear();     // keys mean other things now
            seq_key_ = -1;
            slice_mode_ = on;
        }
        void SetSliceCount(int n) { slices_.SetSliceCount(n); }

        /* CHORALE: the loop itself, from the menu, done in Process() */
        void RequestLoopToggle() { loop_toggle_req_ = true; }
        void RequestLoopClear() { loop_clear_req_ = true; }
        int GetSliceCount() const { return slices_.SliceCount(); }

        /* ---- CHORALE: the sequencer and its clock -------------------- */

        void SetClock(clockManager *c) { clock_ = c; }

        /** a clock step (the sequencer's division, from the audio
         *  callback): the last note lets go, the next one plays */
        void SeqStep()
        {
            if (seq_since_ > 0)
                seq_period_ = seq_since_;
            seq_since_ = 0;
            seq_steps_++;
            if (!seq_.Playing())
                return;
            if (seq_key_ >= 0)
            {
                TargetOff(seq_key_);
                seq_key_ = -1;
            }
            int   key;
            float semis;
            if (seq_.Step(key, semis))
            {
                TargetOn(key, semis);
                seq_key_ = key;
            }
        }

        /** play: start / stop the sequence (held keys go quiet either way;
         *  the sequence, or a press, plays them again) */
        void SetSeqPlay(bool on)
        {
            if (seq_key_ >= 0)
                TargetOff(seq_key_);
            harmonizer.AllOff();
            slices_.AllOff();
            seq_key_ = -1;
            seq_.SetPlay(on);
        }
        bool IsSeqPlaying() const { return seq_.Playing(); }
        void ToggleSeqLatch() { seq_.ToggleLatch(); }
        bool IsSeqLatched() const { return seq_.Latched(); }
        void SeqNextMode() { seq_.NextMode(); }
        int GetSeqMode() const { return seq_.GetMode(); }
        void SeqStepRest(int d) { seq_.StepRest(d); }
        int GetSeqRest() const { return seq_.Rest(); }
        bool IsInSeq(int key) const { return seq_.InSeq(key); }
        int SeqCurrentKey() const { return seq_key_; }
        uint32_t SeqSteps() const { return seq_steps_; }
        /** 0 at a step, rising to 1 by the next */
        float SeqPhase() const
        {
            const float p = float(seq_since_) / float(seq_period_ > 0 ? seq_period_ : 1);
            return p > 1.f ? 1.f : p;
        }

        /* the wheel: tempo, rate (held + turn), tap (Chompi + click) */
        void ChangeTempo(int turns) { if (clock_) clock_->changeTempo(turns); }
        void ChangeSeqRate(int turns) { if (clock_) clock_->changeDiv(turns, kSeqClock); }
        void TapTempo() { if (clock_) clock_->processTapClock(0.f); }

        /** what a key or the sequencer plays: a slice or a harmony */
        void TargetOn(int key, float semis)
        {
            if (slice_mode_)
                slices_.NoteOn(key, int(lroundf(semis)) + 60);
            else
                harmonizer.NoteOn(key, semis);
        }
        void TargetOff(int key)
        {
            if (slice_mode_)
                slices_.NoteOff(key);
            else
                harmonizer.NoteOff(key);
        }
        bool IsSliceMode() const { return slice_mode_; }
        bool HasSlices() const { return slices_.HasSource(); }
        bool IsSlicePlaying(int key) const { return slices_.Playing(key); }

        /** knobs 1-3 in slice mode: page 1 pitch, start, end; page 2
         *  volume, attack, release */
        void SetSliceKnob(int page, int knob, float v)
        {
            slice_vals_[page][knob] = v;
            switch (page * 3 + knob)
            {
                case 0: slices_.SetPitch(v); break;
                case 1: slices_.SetStart(v); break;
                case 2: slices_.SetEnd(v); break;
                case 3: slices_.SetVolume(v); break;
                case 4: slices_.SetAttack(v); break;
                case 5: slices_.SetRelease(v); break;
                default: break;
            }
        }
        float GetSliceKnob(int page, int knob) const { return slice_vals_[page][knob]; }
        int GetStackNotes() { return harmonizer.StackNotes(); }
        int StackPreview(float root, float *notes) { return harmonizer.StackPreview(root, notes); }
        void SetVoicing(int v) { harmonizer.SetVoicing(v); }
        int GetVoicing() { return harmonizer.Voicing(); }
        void SetChordOctave(int o) { harmonizer.SetChordOctave(o); }
        int GetChordOctave() { return harmonizer.ChordOctave(); }
        int GetStackInterval() { return harmonizer.Interval(); }
        bool IsStackedNote(float semis) { return harmonizer.Stacked(semis); }
        void SetDoubler(float val) { harmonizer.SetDoubler(val); }
        bool IsHarmonyKeyHeld(int key) { return harmonizer.Held(key); }
        /* CHORALE: chord mode (toggle), chord type, scale and tonic */
        void SetChordMode(bool on) { harmonizer.SetChordMode(on); }
        bool IsChordMode() { return harmonizer.ChordMode(); }
        void SetChordType(int idx) { harmonizer.SetChordType(idx); }
        int GetChordType() { return harmonizer.ChordType(); }
        void SetScale(int idx) { harmonizer.SetScale(idx); }
        int GetScale() { return harmonizer.GetScale(); }
        void SetTonic(int pc) { harmonizer.SetTonic(pc); }
        int GetTonic() { return harmonizer.Tonic(); }
        bool InScale(int semis) { return harmonizer.InScale(semis); }
        int ChordNotes(float root, float *notes) { return harmonizer.ChordNotes(root, notes); }
        bool IsFrozen() { return harmonizer.Frozen(); }
        void SetSpread(float val) { harmonizer.SetSpread(val); }

        void SetAttack(float val) { harmonizer.SetAttack(val); }

        void SetDecay(float val) { harmonizer.SetRelease(val); }

        // returns the value for the encoder tracking
        void SetGain(float val) { harmonizer.SetLevel(val); }

        void SetInputGain(float gain) 
        {
            ingain_target_ = gain; 
         
            if(input_monitor && in_source == InputSource::RESAMPLE)
                resamp_env_target_ = ingain_target_;
        }
        
        inline void SetMainGain(float gain) { mgain_target_ = gain; }

        /** SING, knob 6 page 2: 0 = only your voice, .5 = both at full
         *  level, 1 = only the harmonies */
        inline void SetDryWet(float val)
        {
            dry_target_ = fminf(1.f, 2.f * (1.f - val));
            wet_target_ = fminf(1.f, 2.f * val);
        }
        inline void SetFinalComp(float comp) { final_lim_target_ = comp; }
        inline float GetFinalComp() { return final_lim_target_; }

        /* CHORALE: TEMPO's delay controls (FxEngine.h). Knob 4 page 1: left
           of centre the random delay, the centre off, right the reverb delay
           with the reverb coming in */
        void SetGranularMain(float val)
        {
            gdelay_->setMainControl(val);
            if (val > .55f)
            {
                const float norm = (val - .55f) / (1.f - .55f);
                reverb_amt_target_   = .25f + .65f * powf(norm, .5f);
                reverb_boost_target_ = val > .6f ? .3f : 0.f;
            }
            else
                reverb_amt_target_ = reverb_boost_target_ = 0.f;
        }
        /** knob 4 page 2: right of centre less dry, left less wet */
        void SetGranularMix(float val)
        {
            if (val >= .5f) { gdry_target_ = 2.f - val * 2.f; gwet_target_ = 1.f; }
            else            { gdry_target_ = 1.f;             gwet_target_ = val * 2.f; }
        }
        void SetGranularAlt(float val) { gdelay_->setAltControl(val); }
        void SetGranularFeedback(float val)
        {
            gdelay_->setFeedback(val);
            gcomp_.setAmount(val > .6f ? (val - .6f) / (1.f - .6f) : 0.f);
        }
        void ToggleGranularFreeze() { gdelay_->toggleBufferLock(); }
        void SetGranularFreeze(bool on) { gdelay_->setBufferLock(on); }
        void GetGranularColors(float *c) { gdelay_->getColors(c); }
        inline void SetWarble(float val) { warble_.SetFreq(val); }

        inline void SetFilter(float val) { cutoff_target_ = val; }
        inline void SetFilterResonance(float val) { res_target_ = val; }
        /** SING, knob 4 page 2: 0 = off, 1 = 6 bits at 3 kHz */
        inline void SetCrush(float val) { crush_target_ = val; }

        inline bool GetLooperRecordArm() { return looper.GetRecordArm(); }
        inline bool GetLooperIsEmpty() { return looper.GetIsEmpty(); }
        inline float GetLooperPosition() { return looper.GetPosition(); }
        inline float GetLooperPitch() { return looper.GetPitch(); }

        inline void LooperRecordButton(bool rising) { looper.RecordButton(rising); }
        inline void LooperPlayButton(bool rising) { looper.PlayButton(rising); }

        inline void ToggleLooperRecord() { looper.ToggleRecord(); }
        inline void ToggleLooperPlaying() { looper.TogglePlaying(); }
        inline bool GetLooperReverse() { return looper.GetReverse(); }

        void ResetLooperPitchQuant()
        {
            looper_fifth = false;
            looper_encoder_chunk = 0.f;
        }

        float SetLooperPitchQuantized(int16_t turns, float enc_pos)
        {
            if (IsLooperPlaying())
            {
                looper_encoder_chunk += turns * .25f;
                if(looper_encoder_chunk >= 1.f || looper_encoder_chunk <= -1.f)
                {
                    // get current semi
                    looper_encoder_chunk = round(looper_encoder_chunk);

                    float pitch = GetLooperPitch();
                    float orig_pitch = pitch;

                    // snap to fifths and octaves
                    // calculate the consts via 2^(x/12) e.g. 2^(-5/12) for down 5 semis
                    float mul;
                    if(pitch > 0.f)
                    {
                        if(looper_fifth)
                            mul = looper_encoder_chunk < 0 ? .667419927085f : 1.33483985417f;
                        else
                            mul = looper_encoder_chunk < 0 ? .749153538438f : 1.49830707688f;
                    }
                    else
                    {
                        if(looper_fifth)
                            mul = looper_encoder_chunk < 0 ? 1.33483985417f : .667419927085f;
                        else
                            mul = looper_encoder_chunk < 0 ? 1.49830707688f : .749153538438f;                            
                    }

                    // snap to next note, return of out of bounds
                    pitch *= mul;
                    if(pitch > 2.f || pitch < -2.f)
                        return enc_pos;

                    looper_fifth = !looper_fifth;
                    looper_encoder_chunk = 0.f;

                    // handle reverse
                    if(pitch < .0625 && pitch > -.0625)
                    {
                        // we weren't already in the turn-around zone
                        if(orig_pitch > .0625 || orig_pitch < -.0625)
                        {
                            looper_fifth = !looper_fifth;
                            pitch = orig_pitch;
                            pitch *= -1.f;
                        }
                        // we were already in the zone, and we're headed over the middle
                        else if((GetLooperReverse() && turns > 0) || (!GetLooperReverse() && turns < 0))
                        {
                            looper_fifth = !looper_fifth;
                            pitch = orig_pitch;
                            pitch *= -1.f;
                        }
                    }

                    SetLooperPitch(pitch);

                    return pitch * .25f + .5f;

                }
            }

            return enc_pos;
        }

        inline void SetLooperPitchFree(float val)
        {
            SetLooperPitch(val * 4.f - 2.f); // -2 - 2            
        }

        inline void SetLooperPitch(float val) { looper.SetPitch(val); }
        inline void SetLooperScrub(float scrub) { looper.SetScrub(scrub); }
        inline float GetLooperScrub() { return looper.GetScrub(); }

        inline bool IsLooperPlaying() { return looper.IsPlaying(); };
        inline bool IsLooperRecording() { return looper.IsRecording(); };
        inline bool IsLooperFirstRecording() { return looper.IsFirstRecording(); };
        inline bool IsLooperRecordArmed() { return looper.GetRecordArm(); };


        inline void IncrementLooperDubGain(float gain) { looper.IncrementDubGain(gain); }
        inline float GetLooperDubGain() { return looper.GetDubGain(); }

        inline void LooperOpenFile() { looper.OpenFile(); }

        daisy::FIFO<KeyRequest, 32> request_fifo;

        // these should really be in a struct defined in the presets file
        float cubbi_pitch;
        float cubbi_start;
        float cubbi_end;
        float cubbi_attack;
        float cubbi_decay;
        bool cubbi_autoloop;
        bool cubbi_sustain;
        float cubbi_gain;
        float cubbi_pan;

        void StopAllVoices(size_t = 100) { harmonizer.AllOff(); }

        // otherwise, they are post looper
        void SetFxPreLooper(bool pre)
        {
            if(pre != fx_pre_loop)
            {
                looper.FXEnvelope();
                fx_env_target_ = 0.f;
            }
        }

        inline bool GetFxPreLooper() { return fx_pre_loop; }

        inline InputSource GetInputSource() { return in_source; }

        void SetInputSource(InputSource source)
        {
            in_source = source;
        }

        /** SING: headphones -> all outputs -> off (the dry voice) */
        inline void IncrementMonitorMode()
        {
            monitor_mode = monitor_mode == MonitorMode::HP     ? MonitorMode::BOTH
                         : monitor_mode == MonitorMode::BOTH   ? MonitorMode::OFF
                                                               : MonitorMode::HP;
        }
        inline MonitorMode GetMonitorMode() { return monitor_mode; }


        // helper for stuck key hack
    private:
 // .0833 = 1/12

        /** the looper */
        LooperEngine looper;

        float encoder_chunk = 0.f; // chunk up the quantized pitch controls
        float looper_encoder_chunk = 0.f;
        bool looper_fifth;
        
        /** env followers for VU meters */
        EnvFollower input_env_follower;
        EnvFollower output_env_follower;

        /** Sampling Bits */

        chompi::Limiter lim_hp_l_;
        chompi::Limiter lim_hp_r_;
        chompi::Limiter lim_line_l_;
        chompi::Limiter lim_line_r_;



        bool input_monitor;



        float ingain_, ingain_target_;
        float resamp_env_, resamp_env_target_;
        float mgain_, mgain_target_;
        float dry_, dry_target_, wet_, wet_target_; // SING: dry/wet
        FilterEnv filter_env_;                      // SING: knobs 2-3, page 2
        SliceEngine      slices_;                   // CHORALE: slice mode
        RamBufferMemory *loop_mem_;
        bool             slice_mode_;
        volatile bool    loop_toggle_req_ = false, loop_clear_req_ = false; // CHORALE
        Sequencer        seq_;                      // CHORALE: the sequencer
        clockManager    *clock_ = nullptr;
        int              seq_key_;                  // the sequenced note sounding, or -1
        uint32_t         seq_since_, seq_period_, seq_steps_;
        static constexpr float kSeqGate = .6f;     // share of a step a note sounds
    public:
        static constexpr size_t kSeqClock = 1;     // the clock division it follows
    private:
        float            slice_vals_[2][3] = {{.5f, 0.f, 1.f}, {.6f, 0.f, .2f}};
        const float *dry_buf_ = nullptr;            // this block's dry gain

        /* SING: resample mode harmonizes the loop: its playback from the
           last block (blocks are 24 samples) */
        static constexpr size_t kLoopBufLen = 128;
        static constexpr float  kLoopGain   = 1.33f; // ~1 at the default input gain
        float loop_buf_[kLoopBufLen] = {};
        float final_lim_, final_lim_target_;

        /** FX */
        MicFilter mic_filter_;
        const float *duck_ = nullptr; // SING: this block's mic ducking

        /* SING: the built-in mic, kMicPreDelay samples late */
        static constexpr int kMicDelayLen = 256;  // power of two
        static constexpr int kMicPreDelay = 96;   // 2 ms
        float        mic_delay_[kMicDelayLen] = {};
        int          mic_delay_w_ = 0;
        const float *mic_d_ = nullptr;           // this block's delayed mic
        DjFilter filter_;
        daisysp::Reverb* reverb_;
        granularDelay   *gdelay_;    // CHORALE: TEMPO's delay (chompi_main.cpp)
        SimpleCompressor gcomp_;
        float gwet_, gwet_target_, gdry_, gdry_target_;
        float reverb_boost_, reverb_boost_target_;
        Warble warble_;
        daisysp::DcBlock dcblock_mic_in_;
        daisysp::DcBlock dcblock_line_in_l_;
        daisysp::DcBlock dcblock_line_in_r_;
        daisysp::DcBlock dcblock_fx_l_;
        daisysp::DcBlock dcblock_fx_r_;


        float reverb_amt_, reverb_amt_target_;
        float crush_, crush_target_, crush_phase_, crush_l_, crush_r_; // SING
        float cutoff_, cutoff_target_;
        float res_, res_target_;
        
        bool fx_pre_loop;
        float fx_env_, fx_env_target_;


        InputSource in_source;
        MonitorMode monitor_mode;

        /** No copy, no assign */
        Engine(const Engine &);
        void operator=(const Engine &);
    };

} // namespace chompi