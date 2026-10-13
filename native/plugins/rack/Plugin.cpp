/*
 * NULL JSFX - Rack (VST3/CLAP), DSP side: up to 8 NULL JSFX modules in series, with per-slot
 * bands and channels, macros, two LFOs and an envelope follower (RackEngine.hpp)
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * Parameters: RackParams.hpp (22 globals, then On/Mix/Band/Channel/Value 1-25 per slot).
 * State: "slots" (8 module ids), "routes" and "preset" (RackParams.hpp). The module in a slot is
 * state, not a parameter: changing it allocates, which happens here, never on the audio thread.
 * Programs: the factory presets (RackPresets.hpp).
 */

#include "RackPlugin.hpp"
#include "NullModules.hpp"

START_NAMESPACE_DISTRHO

class PluginRack : public RackPluginBase
{
public:
    PluginRack()
        : RackPluginBase(nullrack::kModules, nullrack::kNumModules)
    {
        fEngine->setSampleRate(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "Rack"; }
    const char* getDescription() const override
    {
        return "Chains up to 8 NULL JSFX modules, each on a band or channel of its own, with macros, LFOs and an envelope to modulate them.";
    }
    const char* getMaker() const override { return "NULL JSFX"; }
    const char* getHomePage() const override { return "https://nulljsfx.tech"; }
    const char* getLicense() const override { return "MIT"; }
    uint32_t getVersion() const override { return d_version(2, 0, 0); }
    int64_t getUniqueId() const override { return d_cconst('N', 'R', 'c', 'k'); }

    // ---- parameters --------------------------------------------------------------------------

    void initParameter(uint32_t index, Parameter& p) override
    {
        char name[32], sym[32];
        const SliderInfo in = rack::paramInfo(index, name, sizeof(name), sym, sizeof(sym));
        p.hints = kParameterIsAutomatable;
        p.name = name;
        p.shortName = name;
        p.symbol = sym;
        p.unit = in.unit;
        p.ranges.min = in.min;
        p.ranges.max = in.max;
        p.ranges.def = in.def;
        if (in.step >= 1.0f)
            p.hints |= kParameterIsInteger;
        if (index >= rack::kNumGlobals && (index - rack::kNumGlobals) % rack::kSlotParams == rack::kSlotOn)
            p.hints |= kParameterIsBoolean;
        if (in.options)
        {
            const int n = int(in.max - in.min + 0.5f) + 1;
            p.hints |= kParameterIsInteger;
            p.enumValues.count = uint8_t(n);
            p.enumValues.restrictedMode = true;
            p.enumValues.values = new ParameterEnumerationValue[n];
            for (int i = 0; i < n; ++i)
            {
                p.enumValues.values[i].value = in.min + float(i);
                p.enumValues.values[i].label = in.options[i];
            }
        }
    }

    float getParameterValue(uint32_t index) const override { return fEngine->param(index); }
    void setParameterValue(uint32_t index, float v) override { fEngine->setParam(index, v); }

    // ---- programs (factory presets) ----------------------------------------------------------

    void initProgramName(uint32_t index, String& name) override
    {
        if (index < rack::kNumPresets)
            name = rack::kPresets[index].name;
    }

    void loadProgram(uint32_t index) override
    {
        if (index >= rack::kNumPresets)
            return;
        fEngine->loadPreset(rack::kPresets[index]);
        bumpState();
    }

    // ---- state -------------------------------------------------------------------------------

    void initState(uint32_t index, State& s) override
    {
        static const char* const keys[] = { "slots", "routes", "preset" };
        static const char* const labels[] = { "Slots", "Routes", "Preset" };
        s.hints = kStateIsHostReadable;
        s.key = keys[index];
        s.label = labels[index];
        s.defaultValue = index == 0 ? ",,,,,,," : (index == 2 ? "Init" : "");
    }

    String getState(const char* key) const override
    {
        if (std::strcmp(key, "slots") == 0)
        {
            std::string ids[rack::kSlots];
            for (uint32_t i = 0; i < rack::kSlots; ++i)
                ids[i] = fEngine->slotId(i);
            return String(rack::formatSlots(ids).c_str());
        }
        if (std::strcmp(key, "routes") == 0)
        {
            rack::Route r[rack::kMaxRoutes];
            const uint32_t n = fEngine->routes(r);
            return String(rack::formatRoutes(r, n).c_str());
        }
        if (std::strcmp(key, "preset") == 0)
            return String(fEngine->presetName().c_str());
        return String();
    }

    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "slots") == 0)
        {
            std::string ids[rack::kSlots];
            rack::parseSlots(value, ids);
            fEngine->setSlots(ids);
        }
        else if (std::strcmp(key, "routes") == 0)
        {
            rack::Route r[rack::kMaxRoutes];
            const uint32_t n = rack::parseRoutes(value, r);
            fEngine->setRoutes(r, n);
        }
        else if (std::strcmp(key, "preset") == 0)
            fEngine->setPresetName(value);
        else
            return;
        bumpState();
    }

    // ---- audio -------------------------------------------------------------------------------

    void activate() override { fEngine->setSampleRate(getSampleRate()); }
    void sampleRateChanged(double sr) override { fEngine->setSampleRate(sr); }

    void run(const float** inputs, float** outputs, uint32_t frames) override
    {
        // the transport as pumper's Plugin.cpp reads it: the BBT position when the host gives one,
        // else the frame position at the tempo while playing, else 0
        const TimePosition& tp = getTimePosition();
        rack::RackEngine::Time t;
        t.tempo = tp.bbt.beatsPerMinute > 0.0 ? tp.bbt.beatsPerMinute : 120.0;
        t.valid = tp.bbt.valid;
        t.playing = tp.playing;
        if (tp.playing)
        {
            if (tp.bbt.valid && tp.bbt.ticksPerBeat > 0.0)
                t.beat = (tp.bbt.bar - 1) * double(tp.bbt.beatsPerBar) + (tp.bbt.beat - 1) +
                         tp.bbt.tick / tp.bbt.ticksPerBeat;
            else
                t.beat = double(tp.frame) / getSampleRate() * t.tempo / 60.0;
        }
        fEngine->process(inputs[0], inputs[1], outputs[0], outputs[1], frames, t);
        const uint32_t lat = fEngine->latency();
        if (lat != fLatency)
        {
            fLatency = lat;
            setLatency(lat);
        }
    }

private:
    uint32_t fLatency = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginRack)
};

Plugin* createPlugin()
{
    return new PluginRack();
}

END_NAMESPACE_DISTRHO
