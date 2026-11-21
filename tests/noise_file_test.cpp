// NULL JSFX - Noise Reduction's noise profile from a file and in the saved state (4.2).
// SPDX-License-Identifier: MIT
//
// usage: noise_file_test <DATA/Effects/null_jsfx> <dir made by tests/make_noise_wavs.py>
//
// With the fixed noise files of tests/make_noise_wavs.py (44.1 kHz stereo 16-bit, 48 kHz mono
// float, 65 s at 22.05 kHz, and one shorter than a frame) it checks, at 44.1, 48 and 96 kHz:
//   - the profile the JSFX saves (@serialize) after reading a file equals the one the native DSP
//     reads from it (native/plugins/noise_reduction/NoiseFile.hpp; 1e-6 relative), and so does a
//     render of a program through both (1e-5); the GFX JSFX gives the same bytes and samples (0);
//   - a file at the plugin's rate gives exactly the profile Learn gives while the same file
//     plays from its first sample, in the JSFX and in the native DSP;
//   - a file longer than 60 s uses 60 s and says so; a file shorter than a frame, or none, leaves
//     no profile; Use Noise File Off empties the profile, a live Learn replaces the file's;
//   - a saved state comes back without the file (JSFX: a Data folder without it; native: the
//     "noiseprofile" string), at the same rate and at another one (mapped by frequency), and the
//     JSFX and the native DSP then sound the same; an older state (no profile) changes nothing.
// Exit code 0 when every check passes.
//
// build: g++ -O2 -std=c++17 -I <ysfx>/include -I native/common -I native/plugins/noise_reduction
//            tests/noise_file_test.cpp libysfx.a -lpthread -ldl

#include <ysfx.h>

#include "NoiseFile.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

static int g_fail = 0;

static void check(bool ok, const std::string& what)
{
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok)
        ++g_fail;
}

static const char* const kFiles[] = { "a_room_44k_stereo16.wav", "b_room_48k_mono_f32.wav", "c_long_22k_mono16.wav",
                                      "d_short.wav" };
static constexpr uint32_t kBlock = 512;
// slider indices (from 0) of noise_reduction.jsfx
static constexpr uint32_t kLearn = 0, kFile = 9, kGate = 10;

static ysfx_t* load(const std::string& path, const std::string& dataRoot, double sr)
{
    ysfx_config_t* cfg = ysfx_config_new();
    ysfx_register_builtin_audio_formats(cfg);
    ysfx_guess_file_roots(cfg, path.c_str());
    ysfx_set_data_root(cfg, dataRoot.c_str());
    ysfx_t* fx = ysfx_new(cfg);
    ysfx_config_free(cfg);
    if (!ysfx_load_file(fx, path.c_str(), 0) || !ysfx_compile(fx, ysfx_compile_no_gfx))
    {
        check(false, path + " compiles");
        ysfx_free(fx);
        return nullptr;
    }
    ysfx_set_sample_rate(fx, sr);
    ysfx_set_block_size(fx, kBlock);
    ysfx_init(fx);
    ysfx_time_info_t ti {};
    ti.tempo = 120;
    ti.playback_state = ysfx_playback_paused;
    ti.time_signature[0] = 4;
    ti.time_signature[1] = 4;
    ysfx_set_time_info(fx, &ti);
    return fx;
}

using Program = std::vector<double>; // interleaved stereo

// 3 s: the room tone of file a (as read, looped) with a 440 Hz tone and a 1.7 kHz tone in bursts
static Program program(const std::string& dir, double sr, bool tones = true)
{
    nullnr::WavStream w;
    std::vector<double> room;
    if (w.open((dir + "/null_jsfx_noise/" + kFiles[0]).c_str()))
    {
        room.resize(size_t(w.avail()));
        room.resize(size_t(w.read(room.data(), room.size())));
    }
    const size_t n = size_t(3.0 * sr);
    Program p(2 * n);
    const size_t rf = room.size() / 2;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = double(i) / sr;
        const double burst = tones && std::fmod(t, 0.75) < 0.4 ? 0.2 : 0.0;
        const double s = burst * (std::sin(2 * M_PI * 440 * t) + 0.4 * std::sin(2 * M_PI * 1700 * t));
        p[2 * i] = s + (rf ? room[2 * (i % rf)] : 0.0);
        p[2 * i + 1] = s * 0.8 + (rf ? room[2 * (i % rf) + 1] : 0.0);
    }
    return p;
}

// ---- the JSFX -------------------------------------------------------------------------------

static Program renderJsfx(ysfx_t* fx, const Program& in)
{
    Program out(in.size());
    const size_t n = in.size() / 2;
    std::vector<double> l(kBlock), r(kBlock), ol(kBlock), orr(kBlock);
    for (size_t pos = 0; pos < n; pos += kBlock)
    {
        const uint32_t m = uint32_t(std::min<size_t>(kBlock, n - pos));
        for (uint32_t i = 0; i < m; ++i)
        {
            l[i] = in[2 * (pos + i)];
            r[i] = in[2 * (pos + i) + 1];
        }
        const double* ins[2] = { l.data(), r.data() };
        double* outs[2] = { ol.data(), orr.data() };
        ysfx_process_double(fx, ins, outs, 2, 2, m);
        for (uint32_t i = 0; i < m; ++i)
        {
            out[2 * (pos + i)] = ol[i];
            out[2 * (pos + i) + 1] = orr[i];
        }
    }
    return out;
}

// the @serialize data of the JSFX as a SavedProfile (ok = false: no profile saved)
struct JsState {
    std::vector<uint8_t> bytes;
    std::vector<std::pair<uint32_t, double>> sliders;
};

static JsState saveJsfx(ysfx_t* fx)
{
    JsState s;
    ysfx_state_t* st = ysfx_save_state(fx);
    s.bytes.assign(st->data, st->data + st->data_size);
    for (uint32_t i = 0; i < st->slider_count; ++i)
        s.sliders.push_back({ st->sliders[i].index, st->sliders[i].value });
    ysfx_state_free(st);
    return s;
}

static void loadJsfx(ysfx_t* fx, const JsState& s)
{
    std::vector<ysfx_state_slider_t> sl;
    for (const auto& p : s.sliders)
        sl.push_back({ p.first, p.second });
    std::vector<uint8_t> data = s.bytes;
    ysfx_state_t st {};
    st.sliders = sl.data();
    st.slider_count = uint32_t(sl.size());
    st.data = data.data();
    st.data_size = data.size();
    ysfx_load_state(fx, &st);
}

static float f32(const std::vector<uint8_t>& b, size_t i)
{
    return nullnr::getF32(&b[4 * i]);
}

static nullnr::SavedProfile parse(const JsState& s)
{
    nullnr::SavedProfile p;
    const std::vector<uint8_t>& b = s.bytes;
    if (b.size() < 7 * 4 || f32(b, 0) != 1312.0f || f32(b, 1) != 1.0f)
        return p;
    p.src = int(f32(b, 2));
    p.sampleRate = f32(b, 3);
    p.length = f32(b, 4);
    p.gate = f32(b, 5) >= 0.5f;
    p.cut = f32(b, 6) >= 0.5f;
    if (p.src < 1 || p.src > 2)
        return p;
    if (b.size() != (7 + size_t(nullnr::kNB)) * 4)
        return p;
    for (int k = 0; k < nullnr::kNB; ++k)
        p.avg[k] = f32(b, 7 + size_t(k));
    p.ok = true;
    return p;
}

// ---- the native DSP -------------------------------------------------------------------------

struct Native {
    nullnr::NoiseReducer dsp;
    nullnr::ProfileSnapshot snap;
    nullnr::Profiles msg;
    bool pending = false;

    explicit Native(double sr, bool learn = false)
    {
        dsp.setSnapshot(&snap);
        dsp.setSampleRate(sr);
        dsp.setParams(learn, false, 18, 3, 50, 10, 150, false, 100);
    }
    void file(const std::string& path, double sr)
    {
        nullnr::analyseFile(path.c_str(), sr, msg.file);
        ++msg.fileSeq;
        pending = true;
    }
    void saved(const nullnr::SavedProfile& s)
    {
        msg.saved = s;
        ++msg.savedSeq;
        pending = true;
    }
    Program render(const Program& in)
    {
        Program out(in.size());
        const size_t n = in.size() / 2;
        std::vector<double> l(kBlock), r(kBlock), ol(kBlock), orr(kBlock);
        for (size_t pos = 0; pos < n; pos += kBlock)
        {
            const uint32_t m = uint32_t(std::min<size_t>(kBlock, n - pos));
            if (pending)
            {
                dsp.takeProfiles(msg);
                pending = false;
            }
            for (uint32_t i = 0; i < m; ++i)
            {
                l[i] = in[2 * (pos + i)];
                r[i] = in[2 * (pos + i) + 1];
            }
            dsp.process(l.data(), r.data(), ol.data(), orr.data(), m);
            for (uint32_t i = 0; i < m; ++i)
            {
                out[2 * (pos + i)] = ol[i];
                out[2 * (pos + i) + 1] = orr[i];
            }
        }
        return out;
    }
};

// ---- comparisons ----------------------------------------------------------------------------

static double maxDiff(const Program& a, const Program& b)
{
    double m = a.size() == b.size() ? 0.0 : 1e9;
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i)
        m = std::max(m, std::fabs(a[i] - b[i]));
    return m;
}

static double profDiff(const nullnr::SavedProfile& a, const nullnr::SavedProfile& b)
{
    if (a.ok != b.ok || a.src != b.src)
        return 1e9;
    double m = 0.0;
    for (int k = 0; k < nullnr::kNB; ++k)
        m = std::max(m, std::fabs(double(a.avg[k]) - double(b.avg[k])) / std::max(1e-12, std::fabs(double(a.avg[k]))));
    return m;
}

static bool sameBits(const nullnr::SavedProfile& a, const nullnr::SavedProfile& b)
{
    return a.ok && b.ok && std::memcmp(a.avg, b.avg, sizeof(a.avg)) == 0;
}

static double rms(const Program& p, size_t from)
{
    double s = 0.0;
    for (size_t i = 2 * from; i < p.size(); ++i)
        s += p[i] * p[i];
    return std::sqrt(s / double(std::max<size_t>(1, p.size() - 2 * from)));
}

static std::string fmt(const char* f, double a, double b = 0.0)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf), f, a, b);
    return buf;
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::fprintf(stderr, "usage: %s <DATA/Effects/null_jsfx> <noise dir>\n", argv[0]);
        return 2;
    }
    const std::string plain = std::string(argv[1]) + "/noise_reduction.jsfx";
    const std::string gfx = std::string(argv[1]) + "/gfx/noise_reduction_gfx.jsfx";
    const std::string dir = argv[2];
    const std::string noisedir = dir + "/null_jsfx_noise/";
    const std::string empty = dir + "/empty"; // a Data folder without the files
    std::filesystem::create_directories(empty + "/null_jsfx_noise");

    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const Program in = program(dir, sr);
        for (uint32_t f = 0; f < 4; ++f)
        {
            const std::string tag = fmt("%.0f Hz, file ", sr) + kFiles[f] + ": ";
            ysfx_t* js = load(plain, dir, sr);
            ysfx_t* jg = load(gfx, dir, sr);
            if (!js || !jg)
                return 1;
            for (ysfx_t* fx : { js, jg })
            {
                ysfx_slider_set_value(fx, kFile, f);
                ysfx_slider_set_value(fx, kGate, 1);
            }
            const Program oj = renderJsfx(js, in), og = renderJsfx(jg, in);
            const JsState sj = saveJsfx(js), sg = saveJsfx(jg);
            const nullnr::SavedProfile pj = parse(sj);
            check(sj.bytes == sg.bytes && maxDiff(oj, og) == 0.0, tag + "GFX JSFX saves the same profile and sounds the same (0)");

            Native nat(sr);
            nat.dsp.setGate(true);
            nat.file(noisedir + kFiles[f], sr);
            const Program on = nat.render(in);
            const nullnr::SavedProfile pn = nat.snap.read();
            if (f == 3)
            {
                check(!pj.ok && !pn.ok, tag + "shorter than a frame: no profile in either");
                check(maxDiff(oj, on) <= 1e-5, tag + fmt("native sounds the same (%.2g)", maxDiff(oj, on)));
            }
            else
            {
                check(pj.ok && pj.src == 1 && pj.gate, tag + "the JSFX saves a profile from the file, Use Noise File On");
                check(profDiff(pj, pn) <= 1e-6, tag + fmt("native profile matches (%.2g relative)", profDiff(pj, pn)));
                check(maxDiff(oj, on) <= 1e-5, tag + fmt("native sounds the same (%.2g)", maxDiff(oj, on)));
                check(pj.cut == (f == 2) && pn.cut == (f == 2), tag + (f == 2 ? "cut at 60 s" : "not cut"));
                const double secs = f == 0 ? 10.0 : f == 1 ? 8.0 : 60.0;
                check(std::fabs(pj.length - secs) < 0.1, tag + fmt("profile length %.2f s (file %.0f s)", pj.length, secs));
                // the profile works: the room tone alone comes down by several dB against the
                // same render without the file
                const Program room = program(dir, sr, false);
                ysfx_t* on = load(plain, dir, sr);
                ysfx_slider_set_value(on, kFile, f);
                ysfx_slider_set_value(on, kGate, 1);
                ysfx_t* off = load(plain, dir, sr);
                const double ron = rms(renderJsfx(on, room), size_t(sr)), roff = rms(renderJsfx(off, room), size_t(sr));
                ysfx_free(on);
                ysfx_free(off);
                check(ron < 0.5 * roff, tag + fmt("the profile reduces the room tone (%.1f dB)", 20.0 * std::log10(ron / roff)));

                // reload without the file: the saved profile comes back and sounds the same in both
                ysfx_t* jr = load(plain, empty, sr);
                loadJsfx(jr, sj);
                const nullnr::SavedProfile pr = parse(saveJsfx(jr));
                check(sameBits(pj, pr) && pr.src == 1, tag + "reloaded JSFX without the file keeps the saved profile");
                const Program ojr = renderJsfx(jr, in);
                Native nr(sr);
                nr.dsp.setGate(true);
                nullnr::SavedProfile dec;
                check(nullnr::decodeProfile(nullnr::encodeProfile(pj).c_str(), dec) && sameBits(dec, pj),
                      tag + "noiseprofile string round trip");
                nr.file((empty + "/null_jsfx_noise/") + kFiles[f], sr); // the file is gone
                nr.saved(dec);
                const Program onr = nr.render(in);
                check(maxDiff(ojr, onr) <= 1e-5, tag + fmt("reloaded: native sounds the same (%.2g)", maxDiff(ojr, onr)));
                check(sameBits(nr.snap.read(), pj), tag + "reloaded native keeps the saved profile");
                ysfx_free(jr);

                // at another sample rate, without the file: mapped by frequency in both
                const double sr2 = sr == 48000.0 ? 44100.0 : 48000.0;
                const Program in2 = program(dir, sr2);
                ysfx_t* jm = load(plain, empty, sr2);
                loadJsfx(jm, sj);
                const Program ojm = renderJsfx(jm, in2);
                const nullnr::SavedProfile pm = parse(saveJsfx(jm));
                Native nm(sr2);
                nm.dsp.setGate(true);
                nm.saved(dec);
                const Program onm = nm.render(in2);
                check(pm.ok && profDiff(pm, nm.snap.read()) <= 1e-6 && maxDiff(ojm, onm) <= 1e-5,
                      tag + fmt("reloaded at %.0f Hz without the file: mapped, the same in both (%.2g)", sr2, maxDiff(ojm, onm)));
                ysfx_free(jm);
                // ... and with the file: read again at the new rate
                ysfx_t* jf = load(plain, dir, sr2);
                loadJsfx(jf, sj);
                renderJsfx(jf, in2);
                ysfx_t* jn = load(plain, dir, sr2);
                ysfx_slider_set_value(jn, kFile, f);
                ysfx_slider_set_value(jn, kGate, 1);
                renderJsfx(jn, in2);
                check(sameBits(parse(saveJsfx(jf)), parse(saveJsfx(jn))),
                      tag + fmt("reloaded at %.0f Hz with the file: read from the file again", sr2));
                ysfx_free(jf);
                ysfx_free(jn);

                // Use Noise File Off empties the profile (JSFX and native)
                ysfx_slider_set_value(js, kGate, 0);
                renderJsfx(js, in);
                nat.dsp.setGate(false);
                nat.render(in);
                check(!parse(saveJsfx(js)).ok && !nat.snap.read().ok, tag + "Use Noise File Off empties the profile");
            }
            ysfx_free(js);
            ysfx_free(jg);
        }

        // a file at the plugin's rate gives exactly Learn's profile while it plays from its first
        // sample (file a at 44.1 kHz, file b at 48 kHz)
        for (uint32_t f = 0; f < 2; ++f)
        {
            if (sr != (f == 0 ? 44100.0 : 48000.0))
                continue;
            nullnr::WavStream w;
            w.open((noisedir + kFiles[f]).c_str());
            const int nch = w.channels();
            std::vector<double> raw(size_t(w.avail()));
            raw.resize(size_t(w.read(raw.data(), raw.size())));
            // as ysfx and REAPER read it (single precision for float files is exact here)
            Program play(2 * (raw.size() / size_t(nch)) + 2 * kBlock, 0.0);
            for (size_t i = 0; i < raw.size() / size_t(nch); ++i)
            {
                play[2 * i] = raw[i * size_t(nch)];
                play[2 * i + 1] = raw[i * size_t(nch) + (nch > 1 ? 1 : 0)];
            }
            ysfx_t* jl = load(plain, dir, sr);
            ysfx_slider_set_value(jl, kLearn, 1);
            // Learn Off at the first block after the file (the extra block of silence)
            const size_t fileFrames = raw.size() / size_t(nch);
            Program a(play.begin(), play.begin() + 2 * long(fileFrames - fileFrames % kBlock));
            renderJsfx(jl, a);
            Program rest(play.begin() + 2 * long(fileFrames - fileFrames % kBlock), play.end());
            // the last partial block still belongs to the file
            Program tail(rest.begin(), rest.begin() + 2 * long(fileFrames % kBlock));
            renderJsfx(jl, tail);
            ysfx_slider_set_value(jl, kLearn, 0);
            renderJsfx(jl, Program(2 * kBlock, 0.0));
            const nullnr::SavedProfile learnt = parse(saveJsfx(jl));
            ysfx_free(jl);
            nullnr::FileProfile fp;
            nullnr::analyseFile((noisedir + kFiles[f]).c_str(), sr, fp);
            nullnr::SavedProfile fromFile;
            fromFile.ok = fp.ok;
            for (int k = 0; k < nullnr::kNB; ++k)
                fromFile.avg[k] = float(fp.sum[k] / fp.frames);
            check(learnt.ok && learnt.src == 2 && sameBits(learnt, fromFile),
                  fmt("%.0f Hz: ", sr) + kFiles[f] + ": the file's profile is exactly Learn's while it plays");
        }

        // a live Learn replaces the file's profile; an older state (no profile) changes nothing
        {
            ysfx_t* js = load(plain, dir, sr);
            ysfx_slider_set_value(js, kFile, 0);
            ysfx_slider_set_value(js, kGate, 1);
            renderJsfx(js, in);
            ysfx_slider_set_value(js, kLearn, 1);
            renderJsfx(js, in);
            ysfx_slider_set_value(js, kLearn, 0);
            renderJsfx(js, Program(2 * kBlock, 0.0));
            const nullnr::SavedProfile p = parse(saveJsfx(js));
            check(p.ok && p.src == 2 && p.gate, fmt("%.0f Hz: a live Learn replaces the file's profile", sr));
            ysfx_free(js);

            ysfx_t* a = load(plain, dir, sr);
            ysfx_t* b = load(plain, dir, sr);
            JsState old;
            old.sliders = { { 0, 0 }, { 1, 1 }, { 2, 18 } }; // a 4.1 state: no @serialize data
            loadJsfx(b, old);
            ysfx_slider_set_value(a, 1, 1);
            check(maxDiff(renderJsfx(a, in), renderJsfx(b, in)) == 0.0 && !parse(saveJsfx(b)).ok,
                  fmt("%.0f Hz: an older state loads as before (no profile, same sound)", sr));
            ysfx_free(a);
            ysfx_free(b);
        }
    }

    std::printf("noise_file_test: %s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail);
    return g_fail ? 1 : 0;
}
