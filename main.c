/*
  lightning-vis: simple terminal audio visualizer (ALSA + ncurses)
  Controls:
    c - cycle color
    f - toggle orientation (vertical/horizontal)
    q - quit
  Build: gcc -O2 -o lightning-vis main.c -lasound -lncurses -lm
  Notes: Linux only (ALSA). Requires libasound2-dev and libncurses-dev.
*/

#define _DEFAULT_SOURCE
#include <alsa/asoundlib.h>
#include <ncurses.h>
#include <locale.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define SAMPLE_RATE 44100
#define CHANNELS 1
#define FFT_SIZE 1024        // must be power of two
#define BUFFER_FRAMES FFT_SIZE
#define MAX_BARS 512

// Global state
static int orientation_vertical = 1;
static int color_scheme = 0;
static int paused = 0;

// Simple complex arrays for FFT
static double Re[FFT_SIZE];
static double Im[FFT_SIZE];
static double window[FFT_SIZE];

// Bit reversal table
static int rev[FFT_SIZE];

// Precompute window (Hann)
static void init_window() {
    for (int i = 0; i < FFT_SIZE; ++i)
        window[i] = 0.5 * (1.0 - cos(2.0 * M_PI * i / (FFT_SIZE - 1)));
}

// Build bit reversal
static void init_rev() {
    int n = FFT_SIZE;
    int bits = 0;
    while ((1 << bits) < n) bits++;
    for (int i = 0; i < n; ++i) {
        int x = i;
        int y = 0;
        for (int j = 0; j < bits; ++j) {
            y = (y << 1) | (x & 1);
            x >>= 1;
        }
        rev[i] = y;
    }
}

// In-place iterative radix-2 FFT (Cooley-Tukey)
static void fft(double *re, double *im) {
    int n = FFT_SIZE;
    // bit reversal
    for (int i = 0; i < n; ++i) {
        int j = rev[i];
        if (j > i) {
            double tr = re[i]; re[i] = re[j]; re[j] = tr;
            double ti = im[i]; im[i] = im[j]; im[j] = ti;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        double ang = -2.0 * M_PI / len;
        double wlen_cos = cos(ang);
        double wlen_sin = sin(ang);
        for (int i = 0; i < n; i += len) {
            double wr = 1.0, wi = 0.0;
            for (int j = 0; j < (len >> 1); ++j) {
                int u = i + j;
                int v = i + j + (len >> 1);
                double xr = re[u];
                double xi = im[u];
                double yr = re[v] * wr - im[v] * wi;
                double yi = re[v] * wi + im[v] * wr;
                re[u] = xr + yr;
                im[u] = xi + yi;
                re[v] = xr - yr;
                im[v] = xi - yi;
                double tw = wr * wlen_cos - wi * wlen_sin;
                double ti = wr * wlen_sin + wi * wlen_cos;
                wr = tw; wi = ti;
            }
        }
    }
}

// ALSA capture setup
static int setup_alsa(snd_pcm_t **handle) {
    snd_pcm_t *pcm;
    snd_pcm_hw_params_t *params;
    int err;

    if ((err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf(stderr, "ALSA open error: %s\n", snd_strerror(err));
        return err;
    }
    snd_pcm_hw_params_malloc(&params);
    snd_pcm_hw_params_any(pcm, params);
    snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm, params, CHANNELS);
    unsigned int rate = SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(pcm, params, &rate, 0);
    snd_pcm_uframes_t frames = BUFFER_FRAMES;
    snd_pcm_hw_params_set_period_size_near(pcm, params, &frames, 0);
    if ((err = snd_pcm_hw_params(pcm, params)) < 0) {
        fprintf(stderr, "ALSA set params error: %s\n", snd_strerror(err));
        snd_pcm_hw_params_free(params);
        snd_pcm_close(pcm);
        return err;
    }
    snd_pcm_hw_params_free(params);
