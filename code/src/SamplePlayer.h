#pragma once
#include "daisy.h"
#include "daisysp.h"

namespace chompi
{
    /* TEMPO's sample player (SampleManager.h), unchanged: plays 16-bit stereo
       frames from memory at a pitch, with crossfades. CHORALE's slice engine
       uses it on the looper's loop. */
class samplePlayer {
    public:

    samplePlayer() {}
    ~samplePlayer() {}

    static constexpr size_t kMaxCrossfadeLengthLoop = 256;
    static constexpr size_t kMaxCrossfadeLengthClick = 256;

    void Init(float sr) {
        playbackSampleRate_ = sr;
        reverse = false;
        loop = true;
        globalFrequency = 1.f;
    }

    void setStartPoint(float val) {
        size_t offset = static_cast<size_t>(static_cast<float>(num_samples_) * val);
        start_point_ = offset;
        calculateCrossfade();
    }

    void setEndPoint(float val) {
        size_t reduction = static_cast<size_t>(static_cast<float>(num_samples_) * (1.f - val));
        end_point_ = num_samples_ - reduction;
        if (end_point_ < 1) {
            end_point_ = 1; // Ensure at least 1 to avoid indexing issues
        }
        calculateCrossfade();
    }

    void setFrequency(float freq) {
        cur_key_ = freq;
        updateFrequency();
    }

    void setGlobalFrequency(float freq) {
        globalFrequency = freq;
        updateFrequency();
    }

    void updateFrequency() {
        constexpr float middleC = 440.f;
        tuningWord = (cur_key_ * globalFrequency) / middleC;
        if (reverse) {
            tuningWord = -tuningWord;
        }
        calculateCrossfade();
    }

    void setReverse(bool rev) {
        if (reverse != rev) {
            reverse = rev;
            calculateCrossfade();
        }
        reverse = rev;
    }

    void setLoop(bool l) {
        loop = l;
    }

    void resetPlayer(bool still_running) {
        crossfade_counter_ = 0.f;
        if ((still_running && loop) || (still_running && !loop && isVoiceResettable())) {
            crossfading_ = true;
        }
        else {
            crossfading_ = false;
            if (!reverse) {
                phaseAccumulator = static_cast<float>(start_point_);
            }
            else {
                phaseAccumulator = static_cast<float>(end_point_);
            }
        }
        click_crossfading_ = false;
        click_crossfade_counter_ = 0.f;
    }

    void PopStereoSamps(float* left, float* right) {

        if (sample_memory_ == nullptr) {
            // if we get here, there's a bug
            *left = 0.f;
            *right = 0.f;
            return;
        }

        int16_t* pcm_samples = static_cast<int16_t*>(sample_memory_);
        size_t idx = static_cast<size_t>(phaseAccumulator);
        size_t base, next_base;
        float frac;
    
        if (!reverse) {
            base = idx * 2;
            next_base = base + 2;
            if (idx >= end_point_) {
                if (loop) {
                    base = next_base = start_point_ * 2;
                    phaseAccumulator = static_cast<float>(start_point_);
                }
                else if (!crossfading_) {
                    *left = 0.f;
                    *right = 0.f;
                    return;
                }
            }
            frac = phaseAccumulator - static_cast<float>(idx);
        }
        else {
            base = idx * 2;
            next_base = base - 2;
            if (idx <= start_point_) {
                if (loop) {
                    base = next_base = (end_point_ - 1) * 2;
                    phaseAccumulator = static_cast<float>(end_point_ - 1);
                }
                if (!crossfading_) {
                    *left = 0.f;
                    *right = 0.f;
                    return;
                }
            }
            frac = 1.f - (phaseAccumulator - static_cast<float>(idx));
        }

        float crossfadeEnv = 1.f - crossfade_counter_ / static_cast<float>(kMaxCrossfadeLengthLoop);
        float clickEnv = 1.f - click_crossfade_counter_ / static_cast<float>(kMaxCrossfadeLengthClick);
    
        // Interpolate left channel
        int16_t l0 = pcm_samples[base];
        int16_t l1 = pcm_samples[next_base];
        float l_interp = static_cast<float>(l0) + (static_cast<float>(l1 - l0) * frac);
        *left = s162f(static_cast<int16_t>(l_interp)) * crossfadeEnv * clickEnv;
    
        // Interpolate right channel
        int16_t r0 = pcm_samples[base + 1];
        int16_t r1 = pcm_samples[next_base + 1];
        float r_interp = static_cast<float>(r0) + (static_cast<float>(r1 - r0) * frac);
        *right = s162f(static_cast<int16_t>(r_interp)) * crossfadeEnv * clickEnv;

        if (should_crossfade_  && loop) {
            if (phaseAccumulator > crossfade_start_loop_ && !reverse && !crossfading_ || phaseAccumulator < crossfade_start_loop_ && reverse && !crossfading_) {
                crossfading_ = true;
                crossfade_counter_ = 0.f;
            }
        }
        else if (!loop) {
            if (phaseAccumulator > crossfade_start_click_ && !reverse && !click_crossfading_ || phaseAccumulator < crossfade_start_click_ && reverse && !click_crossfading_) {
                click_crossfading_ = true;
                click_crossfade_counter_ = 0.f;
            }
        }
        if (crossfading_) {
            crossfade_counter_ += fabsf(tuningWord);
            float crossfade_pos;
            if (!reverse) {
                crossfade_pos = static_cast<float>(start_point_) + crossfade_counter_;
            }
            else {
                crossfade_pos = static_cast<float>(end_point_) - crossfade_counter_;
            }
            idx = static_cast<size_t>(crossfade_pos);
            base = idx * 2;
            next_base = base + (!reverse ? 2 : -2);
            frac = crossfade_pos - static_cast<float>(idx);
            if (reverse) {
                frac = 1.f - frac;
            }
            l0 = pcm_samples[base];
            l1 = pcm_samples[next_base];
            l_interp = static_cast<float>(l0) + (static_cast<float>(l1 - l0) * frac);
            *left += s162f(static_cast<int16_t>(l_interp)) * (1.f - crossfadeEnv);

            r0 = pcm_samples[base + 1];
            r1 = pcm_samples[next_base + 1];
            r_interp = static_cast<float>(r0) + (static_cast<float>(r1 - r0) * frac);
            *right += s162f(static_cast<int16_t>(r_interp)) * (1.f - crossfadeEnv);
            if (crossfade_counter_ > static_cast<float>(kMaxCrossfadeLengthLoop)) {
                crossfading_ = false;
                crossfade_counter_ = 0;
                phaseAccumulator = crossfade_pos;
            }
        }
        if (click_crossfading_) {
            click_crossfade_counter_ += fabsf(tuningWord);
            if (click_crossfade_counter_ > static_cast<float>(kMaxCrossfadeLengthClick)) {
                click_crossfading_ = false;
                click_crossfade_counter_ = 0.f;
            }
        }
    
        phaseAccumulator += tuningWord;
    }

    void setSample(void *addr, size_t ns) {
        sample_memory_ = addr;
        num_samples_ = ns;
        start_point_ = 0;
        end_point_ = ns;
        calculateCrossfade();
    }

    void calculateCrossfade() {
        window_size_ = end_point_ - start_point_;
        if (window_size_ > 4800) {
            should_crossfade_ = true;
        }
        else {
            should_crossfade_ = false;
        }

        if (!reverse) {
            crossfade_start_loop_ = static_cast<float>(end_point_ - kMaxCrossfadeLengthLoop);
            crossfade_start_click_ = static_cast<float>(end_point_ - kMaxCrossfadeLengthClick);
        }
        else {
            crossfade_start_loop_ = static_cast<float>(start_point_ + kMaxCrossfadeLengthLoop);
            crossfade_start_click_ = static_cast<float>(start_point_ + kMaxCrossfadeLengthClick);
        }
    }

    bool isVoiceResettable() {
        if (phaseAccumulator < crossfade_start_loop_ && !reverse) {
            return true;
        }
        if (phaseAccumulator > crossfade_start_loop_ && reverse) {
            return true;
        }
        return false;
    }

    void *sample_memory_;
    size_t num_samples_;
    size_t start_point_;
    size_t end_point_;
    float cur_key_;
    float phaseAccumulator;
    float tuningWord;
    float playbackSampleRate_;
    float globalFrequency;
    bool reverse;
    bool loop;

    size_t window_size_;
    bool crossfading_, should_crossfade_;
    float crossfade_start_loop_, crossfade_start_click_;
    bool click_crossfading_;
    float crossfade_counter_;
    float click_crossfade_counter_;

    private:
};
} // namespace chompi
