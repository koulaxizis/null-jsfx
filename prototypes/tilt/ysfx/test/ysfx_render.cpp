// Reference renderer: runs a JSFX directly through the ysfx library used by
// tests/ (tests/.build/ysfx, gfx disabled) with the same test signal and
// block size as clap_render.
//
//   ysfx_render <file.jsfx> <slider-name> <value> <out.f32> [srate] [frames] [block]
#include "ysfx.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include "signal.h"

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: %s file.jsfx slider-name value out.f32 [srate] [frames] [block]\n", argv[0]);
        return 2;
    }
    double srate = argc > 5 ? atof(argv[5]) : 48000.0;
    uint32_t frames = argc > 6 ? (uint32_t)atoi(argv[6]) : 48000;
    uint32_t block = argc > 7 ? (uint32_t)atoi(argv[7]) : 512;

    ysfx_config_t *cfg = ysfx_config_new();
    ysfx_t *fx = ysfx_new(cfg);
    ysfx_config_free(cfg);
    if (!ysfx_load_file(fx, argv[1], 0) || !ysfx_compile(fx, ysfx_compile_no_gfx)) {
        fprintf(stderr, "failed to load/compile %s\n", argv[1]);
        return 1;
    }
    ysfx_set_sample_rate(fx, srate);
    ysfx_set_block_size(fx, block);
    ysfx_init(fx);

    int idx = -1;
    for (uint32_t i = 0; i < ysfx_max_sliders; ++i)
        if (ysfx_slider_exists(fx, i) && strcmp(ysfx_slider_get_name(fx, i), argv[2]) == 0)
            idx = (int)i;
    if (idx < 0) { fprintf(stderr, "slider \"%s\" not found\n", argv[2]); return 1; }
    ysfx_slider_set_value(fx, (uint32_t)idx, atof(argv[3]));

    std::vector<float> inL(block), inR(block), outL(block), outR(block), result;
    TestSignal sig;
    for (uint32_t pos = 0; pos < frames; pos += block) {
        uint32_t n = std::min(block, frames - pos);
        for (uint32_t i = 0; i < n; ++i) sig.next(inL[i], inR[i]);
        const float *ins[2] = {inL.data(), inR.data()};
        float *outs[2] = {outL.data(), outR.data()};
        ysfx_process_float(fx, ins, outs, 2, 2, n);
        for (uint32_t i = 0; i < n; ++i) { result.push_back(outL[i]); result.push_back(outR[i]); }
    }
    ysfx_free(fx);

    FILE *f = fopen(argv[4], "wb");
    fwrite(result.data(), sizeof(float), result.size(), f);
    fclose(f);
    printf("ysfx reference: %s=%s, wrote %zu frames to %s\n", argv[2], argv[3], result.size() / 2, argv[4]);
    return 0;
}
