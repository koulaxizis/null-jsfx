// Reference renderer for the web null test: runs a JSFX in ysfx (the engine tests/run.sh
// builds) on a given input and writes the output, so web/test/nulltest.js can compare the
// translated JavaScript sample by sample.
// Usage: render_ref fx.jsfx srate block frames in.f64 out.f64 [N=value ...]
//   in.f64: num_inputs * frames doubles, one channel after another; out.f64 likewise.
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
    for (int a = 7; a < argc; ++a) {
        const char *eq = strchr(argv[a], '=');
        if (!eq) continue;
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
    for (uint64_t n = 0; n < frames; n += block) {
        uint32_t len = (uint32_t)((frames - n) < block ? (frames - n) : block);
        for (uint32_t c = 0; c < nin; ++c) ins[c] = in.data() + (size_t)c * frames + n;
        for (uint32_t c = 0; c < nout; ++c) outs[c] = out.data() + (size_t)c * frames + n;
        ti.time_position = n / sr;
        ti.beat_position = ti.time_position * 2;
        ysfx_set_time_info(fx, &ti);
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
