// Renders a JSFX @gfx section to a PNG with ysfx built with YSFX_GFX=ON.
// Usage: gfx_render plugin.jsfx out.png [width height [slider1 [slider2 ...]]]
// Build: g++ -O2 -std=c++17 -I <ysfx>/include -I <ysfx>/thirdparty/stb gfx_render.cpp <ysfx>/build/libysfx.a \
//          -lfreetype -lfontconfig -lpthread -ldl -o gfx_render     (see build.sh)
#include "ysfx.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static void reporter(intptr_t, ysfx_log_level level, const char *msg)
{
    if (level == ysfx_log_error || level == ysfx_log_warning)
        fprintf(stderr, "%s: %s\n", ysfx_log_level_string(level), msg);
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s plugin.jsfx out.png [width height [slider values...]]\n", argv[0]);
        return 2;
    }
    ysfx_config_t *cfg = ysfx_config_new();
    ysfx_set_log_reporter(cfg, &reporter);
    ysfx_t *fx = ysfx_new(cfg);
    ysfx_config_free(cfg);
    if (!ysfx_load_file(fx, argv[1], 0) || !ysfx_compile(fx, 0)) {
        fprintf(stderr, "cannot load/compile %s\n", argv[1]);
        return 1;
    }
    uint32_t dim[2] = {0, 0};
    ysfx_get_gfx_dim(fx, dim);
    uint32_t w = argc > 4 ? (uint32_t)atoi(argv[3]) : (dim[0] ? dim[0] : 400);
    uint32_t h = argc > 4 ? (uint32_t)atoi(argv[4]) : (dim[1] ? dim[1] : 300);
    ysfx_set_sample_rate(fx, 48000);
    ysfx_set_block_size(fx, 256);
    ysfx_init(fx);
    for (int i = 5, s = 0; i < argc; ++i, ++s)
        ysfx_slider_set_value(fx, (uint32_t)s, atof(argv[i]));
    // run ~1.2 s of a music-like stereo signal (kick every 0.5 s, bass, chord, hats) so
    // @slider/@block/@sample see the values and meters/scopes have something to show
    std::vector<double> i0(256), i1(256), o0(256), o1(256);
    const double *ins[2] = {i0.data(), i1.data()};
    double *outs[2] = {o0.data(), o1.data()};
    uint32_t seed = 1;
    long n = 0;
    for (int b = 0; b < 225; ++b) {
        for (int k = 0; k < 256; ++k, ++n) {
            const double t = n / 48000.0, tk = std::fmod(t, 0.5), th = std::fmod(t, 0.125);
            seed = seed * 1664525u + 1013904223u;
            const double noise = (seed >> 9) / 4194304.0 - 1.0;
            const double kick = 0.7 * std::exp(-tk * 18) * std::sin(2 * M_PI * (50 + 90 * std::exp(-tk * 40)) * tk);
            const double bass = 0.18 * std::sin(2 * M_PI * 55 * t);
            const double chordL = 0.06 * (std::sin(2 * M_PI * 220 * t) + std::sin(2 * M_PI * 277.2 * t) + std::sin(2 * M_PI * 329.6 * t));
            const double chordR = 0.06 * (std::sin(2 * M_PI * 220.7 * t + 0.4) + std::sin(2 * M_PI * 276.6 * t + 1.1) + std::sin(2 * M_PI * 330.4 * t + 2.0));
            const double hat = 0.08 * std::exp(-th * 60) * noise;
            i0[k] = kick + bass + chordL + hat;
            i1[k] = kick + bass + chordR - 0.6 * hat;
        }
        ysfx_process_double(fx, ins, outs, 2, 2, 256);
    }

    std::vector<uint8_t> px((size_t)w * h * 4, 0);
    ysfx_gfx_config_t gc = {};
    gc.pixel_width = w;
    gc.pixel_height = h;
    gc.pixel_stride = 4 * w;
    gc.pixels = px.data();
    gc.scale_factor = 1.0;
    ysfx_gfx_setup(fx, &gc);
    ysfx_gfx_update_mouse(fx, 0, -1, -1, 0, 0, 0);
    for (int i = 0; i < 2; ++i) ysfx_gfx_run(fx);

    // BGRA (little-endian) -> RGBA, opaque
    for (size_t i = 0; i < px.size(); i += 4) {
        uint8_t b = px[i];
        px[i] = px[i + 2];
        px[i + 2] = b;
        px[i + 3] = 255;
    }
    if (!stbi_write_png(argv[2], (int)w, (int)h, 4, px.data(), (int)w * 4)) {
        fprintf(stderr, "cannot write %s\n", argv[2]);
        return 1;
    }
    ysfx_free(fx);
    return 0;
}
