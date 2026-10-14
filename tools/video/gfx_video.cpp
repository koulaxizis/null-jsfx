// Runs a GFX JSFX over an audio file with per-frame slider automation, for the plugin videos.
// Usage: gfx_video plugin.jsfx in.f32 n_in control.f32 n_ctl fps out.f32 [width height]
//   in.f32       interleaved float32 input, n_in channels, 48 kHz
//   control.f32  float32 per video frame: n_ctl slider values (slider1 first)
//   out.f32      interleaved float32 output, every output pin of the plugin
//   width height also renders @gfx once per frame and writes the frames to stdout (RGB24)
// The audio is processed in 256-sample blocks at 120 BPM (the loop's tempo); a frame's
// slider values apply from its first sample. Build: build.sh (next to this file).
#include "ysfx.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

static std::vector<float> read_f32(const char *path)
{
    std::vector<float> v;
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
    float b[4096];
    size_t n;
    while ((n = fread(b, 4, 4096, f)) > 0) v.insert(v.end(), b, b + n);
    fclose(f);
    return v;
}

static void reporter(intptr_t, ysfx_log_level level, const char *msg)
{
    if (level == ysfx_log_error) fprintf(stderr, "%s: %s\n", ysfx_log_level_string(level), msg);
}

int main(int argc, char **argv)
{
    if (argc < 8) {
        fprintf(stderr, "usage: %s plugin.jsfx in.f32 n_in control.f32 n_ctl fps out.f32 [width height]\n", argv[0]);
        return 2;
    }
    const double sr = 48000;
    const uint32_t nin = (uint32_t)atoi(argv[3]), nctl = (uint32_t)atoi(argv[5]);
    const double fps = atof(argv[6]);
    const bool gfx = argc > 9;
    const uint32_t w = gfx ? (uint32_t)atoi(argv[8]) : 0, h = gfx ? (uint32_t)atoi(argv[9]) : 0;
    std::vector<float> in = read_f32(argv[2]), ctl = read_f32(argv[4]);
    const size_t frames = in.size() / nin, vframes = ctl.size() / nctl;

    ysfx_config_t *cfg = ysfx_config_new();
    ysfx_set_log_reporter(cfg, &reporter);
    ysfx_guess_file_roots(cfg, argv[1]);
    ysfx_t *fx = ysfx_new(cfg);
    ysfx_config_free(cfg);
    if (!ysfx_load_file(fx, argv[1], 0) || !ysfx_compile(fx, gfx ? 0 : ysfx_compile_no_gfx)) {
        fprintf(stderr, "cannot load/compile %s\n", argv[1]);
        return 1;
    }
    ysfx_set_sample_rate(fx, sr);
    ysfx_set_block_size(fx, 256);
    ysfx_init(fx);
    const uint32_t pins_in = ysfx_get_num_inputs(fx), pins_out = ysfx_get_num_outputs(fx);

    std::vector<uint8_t> px;
    std::vector<uint8_t> rgb;
    ysfx_gfx_config_t gc = {};
    if (gfx) {
        px.assign((size_t)w * h * 4, 0);
        rgb.resize((size_t)w * h * 3);
        gc.pixel_width = w;
        gc.pixel_height = h;
        gc.pixel_stride = 4 * w;
        gc.pixels = px.data();
        gc.scale_factor = 1.0;
        ysfx_gfx_setup(fx, &gc);
        ysfx_gfx_update_mouse(fx, 0, -1, -1, 0, 0, 0);
    }

    std::vector<std::vector<float>> ib(pins_in, std::vector<float>(256)), ob(pins_out, std::vector<float>(256));
    std::vector<const float *> ip(pins_in);
    std::vector<float *> op(pins_out);
    for (uint32_t c = 0; c < pins_in; ++c) ip[c] = ib[c].data();
    for (uint32_t c = 0; c < pins_out; ++c) op[c] = ob[c].data();
    std::vector<float> out(vframes ? (size_t)(vframes / fps * sr) * pins_out : 0);
    const size_t total = out.size() / (pins_out ? pins_out : 1);
    std::vector<double> last(nctl, 1e300);
    ysfx_time_info_t ti = {120.0, ysfx_playback_playing, 0, 0, {4, 4}};

    size_t pos = 0;
    for (size_t vf = 0; vf < vframes; ++vf) {
        for (uint32_t s = 0; s < nctl; ++s) {
            const double v = ctl[vf * nctl + s];
            if (v != last[s]) { ysfx_slider_set_value(fx, s, v); last[s] = v; }
        }
        const size_t end = std::min(total, (size_t)((vf + 1) / fps * sr));
        while (pos < end) {
            const uint32_t blk = (uint32_t)std::min<size_t>(256, end - pos);
            for (uint32_t c = 0; c < pins_in; ++c)
                for (uint32_t i = 0; i < blk; ++i) {
                    const size_t k = (pos + i) % frames;
                    ib[c][i] = c < nin ? in[k * nin + c] : 0.0f;
                }
            ti.time_position = pos / sr;
            ti.beat_position = ti.time_position * 2.0;
            ysfx_set_time_info(fx, &ti);
            ysfx_process_float(fx, ip.data(), op.data(), pins_in, pins_out, blk);
            for (uint32_t c = 0; c < pins_out; ++c)
                for (uint32_t i = 0; i < blk; ++i) out[(pos + i) * pins_out + c] = ob[c][i];
            pos += blk;
        }
        if (gfx) {
            ysfx_gfx_run(fx);
            for (size_t i = 0, j = 0; i < px.size(); i += 4, j += 3) {
                rgb[j] = px[i + 2];
                rgb[j + 1] = px[i + 1];
                rgb[j + 2] = px[i];
            }
            fwrite(rgb.data(), 1, rgb.size(), stdout);
        }
    }
    if (!gfx) {
        FILE *f = fopen(argv[7], "wb");
        fwrite(out.data(), 4, out.size(), f);
        fclose(f);
        fprintf(stderr, "%u\n", pins_out);
    }
    ysfx_free(fx);
    return 0;
}
