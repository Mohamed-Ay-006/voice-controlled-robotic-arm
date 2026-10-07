/* librosa.feature.mfcc(y, sr=16000, n_mfcc=13, n_fft=512, hop_length=256) re-implementation.
 * librosa >= 0.10 defaults: center=True, pad_mode="constant" (zeros), hann window,
 * 128 slaney mel bands, power=2, power_to_db(ref=1, amin=1e-10, top_db=80), DCT-II ortho.
 * Then the notebook's global normalisation. Tables come from tools/gen_tables.py. */
#include "mfcc.h"
#include "mfcc_tables.h"
#include <math.h>
#include <string.h>

#ifndef MFCC_PAD_REFLECT
#define MFCC_PAD_REFLECT 0   /* set 1 only if your librosa is < 0.10 (reflect padding) */
#endif

static float s_cos[MFCC_N_FFT / 2], s_sin[MFCC_N_FFT / 2];
static float s_re[MFCC_N_FFT], s_im[MFCC_N_FFT];
static float s_db[KWS_MFCC_FRAMES][MFCC_N_MELS];
static int s_ready;

static void init_twiddle(void) {
    for (int i = 0; i < MFCC_N_FFT / 2; i++) {
        double a = -2.0 * M_PI * i / MFCC_N_FFT;
        s_cos[i] = (float)cos(a); s_sin[i] = (float)sin(a);
    }
    s_ready = 1;
}

/* in-place iterative radix-2 complex FFT, N = 512 */
static void fft512(float *re, float *im) {
    const int N = MFCC_N_FFT;
    for (int i = 1, j = 0; i < N; i++) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { float t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (int len = 2; len <= N; len <<= 1) {
        int half = len >> 1, step = N / len;
        for (int i = 0; i < N; i += len)
            for (int k = 0; k < half; k++) {
                float wr = s_cos[k * step], wi = s_sin[k * step];
                int a = i + k, b = a + half;
                float xr = re[b] * wr - im[b] * wi, xi = re[b] * wi + im[b] * wr;
                re[b] = re[a] - xr; im[b] = im[a] - xi;
                re[a] += xr;        im[a] += xi;
            }
    }
}

static inline float sample_at(const float *pcm, int idx) {
    if (idx < 0 || idx >= KWS_CLIP_SAMPLES) {
#if MFCC_PAD_REFLECT
        if (idx < 0) idx = -idx; else idx = 2 * (KWS_CLIP_SAMPLES - 1) - idx;
#else
        return 0.0f;
#endif
    }
    return pcm[idx];
}

void mfcc_compute(const float *pcm, float *out) {
    if (!s_ready) init_twiddle();
    float maxdb = -1e30f;
    for (int t = 0; t < KWS_MFCC_FRAMES; t++) {
        int base = t * MFCC_HOP - MFCC_N_FFT / 2;
        for (int n = 0; n < MFCC_N_FFT; n++) { s_re[n] = sample_at(pcm, base + n) * MFCC_HANN[n]; s_im[n] = 0.0f; }
        fft512(s_re, s_im);
        float pw[MFCC_N_FFT / 2 + 1];
        for (int k = 0; k <= MFCC_N_FFT / 2; k++) pw[k] = s_re[k] * s_re[k] + s_im[k] * s_im[k];
        const float *w = MFCC_MEL_W;
        for (int m = 0; m < MFCC_N_MELS; m++) {
            float acc = 0.0f;
            const uint16_t st = MFCC_MEL_START[m], ct = MFCC_MEL_COUNT[m];
            for (int i = 0; i < ct; i++) acc += w[i] * pw[st + i];
            w += ct;
            float db = 10.0f * log10f(acc > 1e-10f ? acc : 1e-10f);
            s_db[t][m] = db;
            if (db > maxdb) maxdb = db;
        }
    }
    const float floor_db = maxdb - 80.0f;               /* top_db = 80 */
    double sum = 0.0, sum2 = 0.0;
    for (int t = 0; t < KWS_MFCC_FRAMES; t++) {
        for (int m = 0; m < MFCC_N_MELS; m++) if (s_db[t][m] < floor_db) s_db[t][m] = floor_db;
        for (int c = 0; c < MFCC_N_MFCC; c++) {
            const float *d = &MFCC_DCT[c * MFCC_N_MELS];
            float acc = 0.0f;
            for (int m = 0; m < MFCC_N_MELS; m++) acc += d[m] * s_db[t][m];
            out[c * KWS_MFCC_FRAMES + t] = acc;
            sum += acc; sum2 += (double)acc * acc;
        }
    }
    const int n = MFCC_N_MFCC * KWS_MFCC_FRAMES;
    double mean = sum / n, var = sum2 / n - mean * mean;
    float inv = 1.0f / ((float)sqrt(var > 0 ? var : 0) + 1e-6f);
    for (int i = 0; i < n; i++) out[i] = (out[i] - (float)mean) * inv;
}
