/* Plays a stereo float32 file in a loop into two JACK ports and records two others, for the
 * Rack video (make_rack_video.py): the Rack's JACK standalone runs live while its window is
 * filmed. Prints the wall-clock start time; records for <seconds>.
 * Usage: jack_loop in.f32 out.f32 seconds <to-left> <to-right> <from-left> <from-right>
 * Build: cc -O2 jack_loop.c -ljack -o jack_loop */
#include <jack/jack.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static jack_port_t *po[2], *pi[2];
static float *in, *out;
static size_t nin, nout, pos;

static int process(jack_nframes_t n, void *arg)
{
    (void)arg;
    float *o0 = jack_port_get_buffer(po[0], n), *o1 = jack_port_get_buffer(po[1], n);
    float *i0 = jack_port_get_buffer(pi[0], n), *i1 = jack_port_get_buffer(pi[1], n);
    for (jack_nframes_t k = 0; k < n; ++k, ++pos) {
        const size_t j = pos % nin;
        o0[k] = in[2 * j];
        o1[k] = in[2 * j + 1];
        if (pos < nout) {
            out[2 * pos] = i0[k];
            out[2 * pos + 1] = i1[k];
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 8) return 2;
    FILE *f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    nin = ftell(f) / 8;
    fseek(f, 0, SEEK_SET);
    in = malloc(nin * 8);
    if (fread(in, 8, nin, f) != nin) return 1;
    fclose(f);
    jack_client_t *c = jack_client_open("jack_loop", JackNoStartServer, NULL);
    if (!c) { fprintf(stderr, "no JACK server\n"); return 1; }
    nout = (size_t)(atof(argv[3]) * jack_get_sample_rate(c));
    out = calloc(nout * 2, sizeof(float));
    po[0] = jack_port_register(c, "out_l", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    po[1] = jack_port_register(c, "out_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
    pi[0] = jack_port_register(c, "in_l", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    pi[1] = jack_port_register(c, "in_r", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    jack_set_process_callback(c, process, NULL);
    pos = 0;
    if (jack_activate(c)) return 1;
    jack_connect(c, "jack_loop:out_l", argv[4]);
    jack_connect(c, "jack_loop:out_r", argv[5]);
    jack_connect(c, argv[6], "jack_loop:in_l");
    jack_connect(c, argv[7], "jack_loop:in_r");
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    const size_t p0 = pos;
    printf("%.3f %zu\n", ts.tv_sec + ts.tv_nsec / 1e9, p0);
    fflush(stdout);
    while (pos < nout) usleep(20000);
    jack_deactivate(c);
    jack_client_close(c);
    f = fopen(argv[2], "wb");
    fwrite(out, 8, nout, f);
    fclose(f);
    return 0;
}
