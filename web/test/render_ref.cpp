// Reference renderer for the web null test: runs a JSFX in ysfx (the engine tests/run.sh
// builds) on a given input and writes the output, so web/test/nulltest.js can compare the
// translated JavaScript sample by sample.
// Usage: render_ref fx.jsfx srate block frames in.f64 out.f64 [N=value ...] [midi=events.txt]
//   in.f64: num_inputs * frames doubles, one channel after another; out.f64 likewise.
//   events.txt: one MIDI message per line, "frame status data1 data2", in time order.
#include "ysfx.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void reporter(intptr_t, ysfx_log_level level, const char *msg)
{
    if (level == ysfx_log_error) fprintf(stderr, "%s: %s\n", ysfx_log_level_string(level), msg);
}

int main(int argc, char **argv)
{
    if (argc < 7) { fprintf(stderr, "usage: render_ref fx.jsfx srate block frames in.f64 out.f64 [N=value ...]\n"); return 2; }
    double sr = atof(argv[2]);
    uint32_t block = (uint32_t)atoi(argv[3]);
    uint64_t frames = (uint64_t)atoll(argv[4]);
    ysfx_config_t *cfg = ysfx_config_new();
    ysfx_guess_file_roots(cfg, argv[1]);
    ysfx_register_builtin_audio_formats(cfg);
    ysfx_set_log_reporter(cfg, &reporter);
    ysfx_t *fx = ysfx_new(cfg);
    if (!ysfx_load_file(fx, argv[1], 0) || !ysfx_compile(fx, 0)) return 3;
    uint32_t nin = ysfx_get_num_inputs(fx), nout = ysfx_get_num_outputs(fx);
    ysfx_set_sample_rate(fx, sr);
    ysfx_set_block_size(fx, block);
    struct Ev { uint64_t at; uint8_t m[3]; };
    std::vector<Ev> midi;
    for (int a = 7; a < argc; ++a) {
        const char *eq = strchr(argv[a], '=');
        if (!eq) continue;
        if (strncmp(argv[a], "midi=", 5) == 0) {
            FILE *mf = fopen(eq + 1, "r");
            if (!mf) { fprintf(stderr, "bad midi file\n"); return 5; }
            unsigned long long at; unsigned s0, s1, s2;
            while (fscanf(mf, "%llu %u %u %u", &at, &s0, &s1, &s2) == 4) midi.push_back({at, {(uint8_t)s0, (uint8_t)s1, (uint8_t)s2}});
            fclose(mf);
            continue;
        }
        ysfx_slider_set_value(fx, (uint32_t)atoi(argv[a]) - 1, atof(eq + 1));
    }
    ysfx_init(fx);
    std::vector<double> in((size_t)nin * frames), out((size_t)nout * frames);
    FILE *f = fopen(argv[5], "rb");
    if (!f || fread(in.data(), sizeof(double), in.size(), f) != in.size()) { fprintf(stderr, "bad input\n"); return 4; }
    fclose(f);
    std::vector<const double *> ins(nin);
    std::vector<double *> outs(nout);
    ysfx_time_info_t ti = {120.0, ysfx_playback_playing, 0, 0, {4, 4}};
    size_t next = 0;
    for (uint64_t n = 0; n < frames; n += block) {
        uint32_t len = (uint32_t)((frames - n) < block ? (frames - n) : block);
        for (uint32_t c = 0; c < nin; ++c) ins[c] = in.data() + (size_t)c * frames + n;
        for (uint32_t c = 0; c < nout; ++c) outs[c] = out.data() + (size_t)c * frames + n;
        ti.time_position = n / sr;
        ti.beat_position = ti.time_position * 2;
        ysfx_set_time_info(fx, &ti);
        while (next < midi.size() && midi[next].at < n + len) {
            ysfx_midi_event_t ev = {0, (uint32_t)(midi[next].at - n), 3, midi[next].m};
            ysfx_send_midi(fx, &ev);
            ++next;
        }
        ysfx_process_double(fx, ins.data(), outs.data(), nin, nout, len);
    }
    f = fopen(argv[6], "wb");
    fwrite(out.data(), sizeof(double), out.size(), f);
    fclose(f);
    printf("%u %u %g\n", nin, nout, ysfx_get_pdc_delay(fx));
    ysfx_free(fx);
    ysfx_config_free(cfg);
    return 0;
}
