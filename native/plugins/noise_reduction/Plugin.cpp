/*
 * NULL JSFX - Noise Reduction (VST3/CLAP), DSP side
 * Copyright (C) 2026 Christos Koulaxizis / NULL JSFX
 * SPDX-License-Identifier: MIT
 *
 * 4.2: the profile from a file and in the saved state. States: "noisefile" (the Noise File's
 * path, like Convolution's "irfile") and "noiseprofile" (the profile in use, NoiseFile.hpp).
 * The file is read and analysed here in setState() / sampleRateChanged(), never on the audio
 * thread, and handed to process() without a lock (NullMailbox.hpp); process() applies it at the
 * block start like the JSFX's @block. A loaded "noiseprofile" wins over the file, as the JSFX's
 * @serialize does.
 */

#include "NoiseReductionSliders.hpp"
#include "NoiseFile.hpp"
#include "NullMailbox.hpp"

#include <cstring>
#include <mutex>
#include <string>

START_NAMESPACE_DISTRHO

class PluginNoiseReduction : public NoiseReductionBase
{
public:
    PluginNoiseReduction()
    {
        fDsp.setView(&view);
        fDsp.setSnapshot(&fSnap);
        init(getSampleRate());
    }

protected:
    const char* getLabel() const override { return "NoiseReduction"; }
    const char* getDescription() const override
    {
        return "Stationary noise reducer: learns a noise profile (live or from a file) or tracks the noise floor, "
               "Wiener-like spectral gain.";
    }
    int64_t getUniqueId() const override { return d_cconst('N', 'N', 'R', 'd'); }

    void valueChanged(uint32_t, float) override { update(); }

    // like the JSFX's @init: the learnt profile survives
    void onActivate() override { init(getSampleRate()); }

    void sampleRateChanged(double newSampleRate) override
    {
        init(newSampleRate);
        // a profile from a file is read from the file again at the new rate
        std::lock_guard<std::mutex> lock(fStateLock);
        analyse();
        post();
    }

    String getState(const char* key) const override
    {
        if (std::strcmp(key, "noisefile") == 0)
        {
            std::lock_guard<std::mutex> lock(fStateLock);
            return String(fPath.c_str());
        }
        if (std::strcmp(key, "noiseprofile") == 0)
            return String(nullnr::encodeProfile(fSnap.read()).c_str());
        return String();
    }

    void setState(const char* key, const char* value) override
    {
        std::lock_guard<std::mutex> lock(fStateLock);
        if (std::strcmp(key, "noisefile") == 0)
        {
            fPath = value ? value : "";
            analyse();
            ++fMsg.fileSeq; // a new file was chosen (or loaded): slider10 changed
        }
        else if (std::strcmp(key, "noiseprofile") == 0)
        {
            // an empty or damaged string is no saved profile: nothing changes, as in the JSFX
            nullnr::SavedProfile s;
            if (!nullnr::decodeProfile(value, s))
                return;
            fMsg.saved = s;
            ++fMsg.savedSeq;
        }
        else
            return;
        post();
    }

    void process(const float** inputs, float** outputs, uint32_t frames) override
    {
        if (nullnr::Profiles* m = fMail.take())
        {
            fDsp.takeProfiles(*m);
            fMail.giveBack(m);
        }
        fDsp.process(inputs[0], inputs[1], outputs[0], outputs[1], frames);
    }

private:
    void init(double sr)
    {
        fDsp.setSampleRate(sr);
        setLatency(uint32_t(nullnr::NoiseReducer::latency())); // pdc_delay = N - 1
        update();
    }

    void update()
    {
        // Threshold as the JSFX slider holds it (min + k * 0.5 in double)
        const double thr = -10.0 + std::round((double(value(3)) + 10.0) / 0.5) * 0.5;
        fDsp.setParams(value(0) >= 0.5f, value(1) >= 0.5f, std::round(value(2)), thr, std::round(value(4)),
                       std::round(value(5)), std::round(value(6)), value(7) >= 0.5f, std::round(value(8)));
        fDsp.setGate(value(9) >= 0.5f);
    }

    // the Noise File's profile at the current sample rate (fStateLock held)
    void analyse() { nullnr::analyseFile(fPath.c_str(), getSampleRate(), fMsg.file); }

    // hand the audio thread a copy of everything (the newest message wins; fStateLock held)
    void post() { fMail.post(new nullnr::Profiles(fMsg)); }

    nullnr::NoiseReducer fDsp;
    nullnr::ProfileSnapshot fSnap;
    nulljsfx::Mailbox<nullnr::Profiles> fMail;
    mutable std::mutex fStateLock;
    nullnr::Profiles fMsg; // what was last posted (fStateLock)
    std::string fPath;     // the Noise File (fStateLock)

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginNoiseReduction)
};

Plugin* createPlugin()
{
    return new PluginNoiseReduction();
}

END_NAMESPACE_DISTRHO
