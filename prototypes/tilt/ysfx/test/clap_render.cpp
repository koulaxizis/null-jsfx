// Minimal headless CLAP host: loads a .clap, prints its descriptor and
// parameters, sets one parameter (by name, as display text) and renders a
// deterministic stereo test signal through it.
//
//   clap_render <plugin.clap> <param-name> <param-text> <out.f32> [srate] [frames] [block]
//
// Output: raw interleaved float32 stereo. test/signal.h defines the input.
#include <clap/clap.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "signal.h"

static const void *hostGetExtension(const clap_host_t *, const char *) { return nullptr; }
static void hostNoop(const clap_host_t *) {}

static uint32_t inSize(const clap_input_events_t *l) { return *(const uint32_t *)((const void *const *)l->ctx)[1]; }
static const clap_event_header_t *inGet(const clap_input_events_t *l, uint32_t i)
{
    auto ev = (const clap_event_param_value_t *)((const void *const *)l->ctx)[0];
    return &ev[i].header;
}
static bool outPush(const clap_output_events_t *, const clap_event_header_t *) { return true; }

int main(int argc, char **argv)
{
    if (argc < 5) {
        fprintf(stderr, "usage: %s plugin.clap param-name param-text out.f32 [srate] [frames] [block]\n", argv[0]);
        return 2;
    }
    const char *path = argv[1];
    std::string paramName = argv[2];
    const char *paramText = argv[3];
    const char *outPath = argv[4];
    double srate = argc > 5 ? atof(argv[5]) : 48000.0;
    uint32_t frames = argc > 6 ? (uint32_t)atoi(argv[6]) : 48000;
    uint32_t block = argc > 7 ? (uint32_t)atoi(argv[7]) : 512;

    void *lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
    auto entry = (const clap_plugin_entry_t *)dlsym(lib, "clap_entry");
    if (!entry) { fprintf(stderr, "no clap_entry\n"); return 1; }
    if (!entry->init(path)) { fprintf(stderr, "entry init failed\n"); return 1; }
    auto factory = (const clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    if (!factory || factory->get_plugin_count(factory) < 1) { fprintf(stderr, "no plugins\n"); return 1; }
    const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, 0);
    printf("clap: id=%s name=\"%s\" vendor=\"%s\" version=%s features=", desc->id, desc->name, desc->vendor, desc->version);
    for (const char *const *f = desc->features; f && *f; ++f) printf("%s ", *f);
    printf("\n");

    clap_host_t host{};
    host.clap_version = CLAP_VERSION;
    host.name = "clap_render";
    host.vendor = "NULL JSFX";
    host.version = "1";
    host.get_extension = hostGetExtension;
    host.request_restart = hostNoop;
    host.request_process = hostNoop;
    host.request_callback = hostNoop;

    const clap_plugin_t *plugin = factory->create_plugin(factory, &host, desc->id);
    if (!plugin || !plugin->init(plugin)) { fprintf(stderr, "create/init failed\n"); return 1; }

    auto params = (const clap_plugin_params_t *)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
    auto ports = (const clap_plugin_audio_ports_t *)plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS);
    if (ports)
        printf("audio ports: %u in, %u out\n", ports->count(plugin, true), ports->count(plugin, false));

    clap_id targetId = CLAP_INVALID_ID;
    void *targetCookie = nullptr;
    double targetValue = 0;
    uint32_t nparams = params ? params->count(plugin) : 0;
    printf("params: %u\n", nparams);
    for (uint32_t i = 0; i < nparams; ++i) {
        clap_param_info_t info{};
        params->get_info(plugin, i, &info);
        char txt[64] = {};
        params->value_to_text(plugin, info.id, info.default_value, txt, sizeof(txt));
        printf("  [%u] id=%u \"%s\" min=%g max=%g default=%g (\"%s\") automatable=%d\n", i, info.id, info.name,
               info.min_value, info.max_value, info.default_value, txt, (info.flags & CLAP_PARAM_IS_AUTOMATABLE) != 0);
        if (paramName == info.name) {
            targetId = info.id;
            targetCookie = info.cookie;
            if (!params->text_to_value(plugin, info.id, paramText, &targetValue)) {
                fprintf(stderr, "text_to_value failed\n");
                return 1;
            }
        }
    }
    printf("setting \"%s\" to text \"%s\" -> clap value %g\n", paramName.c_str(), paramText, targetValue);
    if (targetId == CLAP_INVALID_ID) { fprintf(stderr, "param \"%s\" not found\n", paramName.c_str()); return 1; }

    if (!plugin->activate(plugin, srate, 1, block)) { fprintf(stderr, "activate failed\n"); return 1; }
    if (!plugin->start_processing(plugin)) { fprintf(stderr, "start_processing failed\n"); return 1; }

    std::vector<float> inL(block), inR(block), outL(block), outR(block), result;
    result.reserve((size_t)frames * 2);
    TestSignal sig;
    clap_event_param_value_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.time = 0;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.param_id = targetId;
    ev.cookie = targetCookie;
    ev.note_id = -1; ev.port_index = -1; ev.channel = -1; ev.key = -1;
    ev.value = targetValue;

    for (uint32_t pos = 0; pos < frames; pos += block) {
        uint32_t n = std::min(block, frames - pos);
        for (uint32_t i = 0; i < n; ++i) sig.next(inL[i], inR[i]);
        float *inPtr[2] = {inL.data(), inR.data()};
        float *outPtr[2] = {outL.data(), outR.data()};
        clap_audio_buffer_t ain{}, aout{};
        ain.data32 = inPtr; ain.channel_count = 2;
        aout.data32 = outPtr; aout.channel_count = 2;
        uint32_t evCount = pos == 0 ? 1 : 0;
        const void *ctx[2] = {&ev, &evCount};
        clap_input_events_t inEv{ctx, inSize, inGet};
        clap_output_events_t outEv{nullptr, outPush};
        clap_process_t proc{};
        proc.steady_time = pos;
        proc.frames_count = n;
        proc.audio_inputs = &ain; proc.audio_inputs_count = 1;
        proc.audio_outputs = &aout; proc.audio_outputs_count = 1;
        proc.in_events = &inEv; proc.out_events = &outEv;
        if (plugin->process(plugin, &proc) == CLAP_PROCESS_ERROR) { fprintf(stderr, "process error\n"); return 1; }
        for (uint32_t i = 0; i < n; ++i) { result.push_back(outL[i]); result.push_back(outR[i]); }
    }
    double v = 0;
    params->get_value(plugin, targetId, &v);
    char txt[64] = {};
    params->value_to_text(plugin, targetId, v, txt, sizeof(txt));
    printf("after render: %s = %s\n", paramName.c_str(), txt);

    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    entry->deinit();

    FILE *f = fopen(outPath, "wb");
    fwrite(result.data(), sizeof(float), result.size(), f);
    fclose(f);
    printf("wrote %zu frames to %s\n", result.size() / 2, outPath);
    // dlclose intentionally skipped (JUCE statics).
    return 0;
}
