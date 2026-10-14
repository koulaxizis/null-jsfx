// Plays raw audio through one NULL JSFX for the plugin demos (tools/render_demos.py).
// Reads interleaved stereo 32-bit float from stdin, writes the same to stdout.
// Usage: demo_render <plugin.jsfx> <sample rate> [index=value ...] [seconds@index=value ...]
// index=value sets a slider (index from 0) before the start; seconds@index=value moves it at
// that time, as a user would (Tape Stop's trigger, Spectral Freeze's freeze).
// Plugins with sidechain pins (inputs 3/4) get the input's mono sum there.
#include "ysfx.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

struct Move { double at; uint32_t slider; double value; };

static void reporter(intptr_t, ysfx_log_level level, const char *msg)
{
    if (level == ysfx_log_error)
        std::fprintf(stderr, "%s: %s\n", ysfx_log_level_string(level), msg);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        std::fprintf(stderr, "usage: demo_render <plugin.jsfx> <rate> [index=value ...]\n");
        return 2;
    }
    ysfx_config_t *cfg = ysfx_config_new();
    ysfx_guess_file_roots(cfg, argv[1]);
    ysfx_set_log_reporter(cfg, &reporter);
    ysfx_t *fx = ysfx_new(cfg);
    ysfx_config_free(cfg);
    if (!ysfx_load_file(fx, argv[1], 0) || !ysfx_compile(fx, ysfx_compile_no_gfx)) {
        std::fprintf(stderr, "demo_render: %s does not compile\n", argv[1]);
        return 1;
    }
    const double sr = std::atof(argv[2]);
    const uint32_t block = 256;
    ysfx_set_sample_rate(fx, sr);
    ysfx_set_block_size(fx, block);
    ysfx_init(fx);
    std::vector<Move> moves;
    for (int i = 3; i < argc; ++i) {
        const char *eq = std::strchr(argv[i], '=');
        const char *at = std::strchr(argv[i], '@');
        if (!eq)
            continue;
        if (at && at < eq)
            moves.push_back({std::atof(argv[i]), (uint32_t)std::atoi(at + 1), std::atof(eq + 1)});
        else
            ysfx_slider_set_value(fx, (uint32_t)std::atoi(argv[i]), std::atof(eq + 1));
    }
    ysfx_init(fx); // @init sees the slider values, as when REAPER loads a preset

    const uint32_t nin = ysfx_get_num_inputs(fx) > 2 ? 4 : 2;
    std::vector<float> io(block * 2);
    std::vector<double> l(block), r(block), sc(block), ol(block), orr(block);
    const double *ins[4] = {l.data(), r.data(), sc.data(), sc.data()};
    double *outs[2] = {ol.data(), orr.data()};
    ysfx_time_info_t ti = {120.0, ysfx_playback_playing, 0, 0, {4, 4}};
    size_t n;
    while ((n = std::fread(io.data(), sizeof(float) * 2, block, stdin)) > 0) {
        for (size_t i = 0; i < block; ++i) {
            l[i] = i < n ? io[2 * i] : 0;
            r[i] = i < n ? io[2 * i + 1] : 0;
            sc[i] = 0.5 * (l[i] + r[i]);
        }
        for (const Move &m : moves)
            if (m.at >= ti.time_position && m.at < ti.time_position + block / sr)
                ysfx_slider_set_value(fx, m.slider, m.value);
        ysfx_set_time_info(fx, &ti);
        ysfx_process_double(fx, ins, outs, nin, 2, block);
        ti.time_position += block / sr;
        ti.beat_position += block / sr * 2.0;
        for (size_t i = 0; i < n; ++i) {
            io[2 * i] = (float)ol[i];
            io[2 * i + 1] = (float)orr[i];
        }
        std::fwrite(io.data(), sizeof(float) * 2, n, stdout);
    }
    ysfx_free(fx);
    return 0;
}
