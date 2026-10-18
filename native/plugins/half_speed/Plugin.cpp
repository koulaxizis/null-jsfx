/*
 * NULL JSFX - Half Speed (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 */

#include "NullPlugin.hpp"
#include "HalfSpeedDSP.hpp"
#include "HalfSpeedSliders.hpp"

START_NAMESPACE_DISTRHO

class PluginHalfSpeed : public HalfSpeedBase
{
public:
    PluginHalfSpeed()
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

protected:
    const char* getLabel() const override { return "HalfSpeed"; }
    const char* getDescription() const override
    {
        return "Real-time halftime locked to the host tempo: plays each Length of input at half (or a quarter, or two thirds) speed, with an optional granular mode that keeps the pitch, a tone control and mix.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'H', 's', 'p'); }

    void valueChanged(uint32_t, float) override { update(); }

    void activate() override
    {
        fDsp.setSampleRate(getSampleRate());
        update();
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fDsp.setSampleRate(newSampleRate);
        update();
    }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        const TimePosition& tp = getTimePosition();
        const double tempo = tp.bbt.valid && tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        double beatPos = 0.0;
        if (tp.playing)
        {
            if (tp.bbt.valid && tp.bbt.ticksPerBeat > 0.0)
                beatPos = (tp.bbt.bar - 1) * double(tp.bbt.beatsPerBar) + (tp.bbt.beat - 1) +
                          tp.bbt.tick / tp.bbt.ticksPerBeat;
            else
                beatPos = double(tp.frame) / getSampleRate() * tempo / 60.0;
            // REAPER re-runs the JSFX's @init when the transport starts
            if (!fWasPlaying)
                fDsp.reset();
        }
        fWasPlaying = tp.playing;
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames, tempo, tp.playing, beatPos);

        const double ls = std::fmax(1.0, fDsp.capture()), cl = std::fmax(1.0, fDsp.cycle()), n = fDsp.pos();
        view.write.store(float(std::fmin(n, ls) / ls), std::memory_order_relaxed);
        view.read.store(float(std::fmin(1.0, fDsp.speed() * n / ls)), std::memory_order_relaxed);
        view.cycle.store(float(std::fmin(1.0, n / cl)), std::memory_order_relaxed);
        view.capture.store(float(std::fmin(1.0, ls / cl)), std::memory_order_relaxed);
        for (int b = 0; b < nullhalfspeed::HalfSpeedProcessor::kBins; ++b)
            view.peaks[b].store(float(fDsp.peak(b)), std::memory_order_relaxed);
    }

private:
    void update()
    {
        fDsp.setParams(nullhalfspeed::paramsFrom([this](uint32_t i) { return value(i); }));
    }

    nullhalfspeed::HalfSpeedProcessor fDsp;
    bool fWasPlaying = false;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginHalfSpeed)
};

Plugin* createPlugin()
{
    return new PluginHalfSpeed();
}

END_NAMESPACE_DISTRHO
