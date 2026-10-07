/* TinyKWSNet (DS-CNN) forward pass, float32, BatchNorm already folded into conv weights.
 * Layout C x H x W with H = 13 MFCC coefficients, W = time frames. Verified against onnxruntime. */
#include "kws_net.h"
#include "kws_weights.h"
#include "mfcc.h"
#include <string.h>

static float *A, *B;
int kws_num_classes(void) { return KWS_NUM_CLASSES; }
const char *kws_label(int i) { return KWS_LABELS[i]; }
void kws_net_set_work(float *a, float *b) { A = a; B = b; }

static inline float relu(float x) { return x > 0.0f ? x : 0.0f; }

/* full 3x3 conv, pad 1, stride 1, cin=1 (stem) */
static void stem(const float *in, float *out, const float *w, const float *bias, int cout, int H, int W) {
    for (int co = 0; co < cout; co++) {
        const float *k = w + co * 9;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                float acc = bias[co];
                for (int ky = 0; ky < 3; ky++) {
                    int iy = y + ky - 1; if (iy < 0 || iy >= H) continue;
                    for (int kx = 0; kx < 3; kx++) {
                        int ix = x + kx - 1; if (ix < 0 || ix >= W) continue;
                        acc += k[ky * 3 + kx] * in[iy * W + ix];
                    }
                }
                out[(co * H + y) * W + x] = relu(acc);
            }
    }
}

/* depthwise 3x3, pad 1, stride (1, sw) */
static void dw3x3(const float *in, float *out, const float *w, const float *bias, int C, int H, int W, int sw, int Wo) {
    for (int c = 0; c < C; c++) {
        const float *k = w + c * 9, *ip = in + c * H * W;
        float *op = out + c * H * Wo;
        for (int y = 0; y < H; y++)
            for (int xo = 0; xo < Wo; xo++) {
                float acc = bias[c];
                int x0 = xo * sw - 1;
                for (int ky = 0; ky < 3; ky++) {
                    int iy = y + ky - 1; if (iy < 0 || iy >= H) continue;
                    for (int kx = 0; kx < 3; kx++) {
                        int ix = x0 + kx; if (ix < 0 || ix >= W) continue;
                        acc += k[ky * 3 + kx] * ip[iy * W + ix];
                    }
                }
                op[y * Wo + xo] = relu(acc);
            }
    }
}

/* pointwise 1x1: out[co][p] = relu(bias + sum_ci w[co][ci] * in[ci][p]) */
static void pw1x1(const float *in, float *out, const float *w, const float *bias, int cin, int cout, int P) {
    for (int co = 0; co < cout; co++) {
        float *o = out + co * P;
        const float *wr = w + co * cin;
        for (int p = 0; p < P; p++) o[p] = bias[co];
        for (int ci = 0; ci < cin; ci++) {
            const float wv = wr[ci]; const float *ip = in + ci * P;
            for (int p = 0; p < P; p++) o[p] += wv * ip[p];
        }
        for (int p = 0; p < P; p++) o[p] = relu(o[p]);
    }
}

void kws_net_run(const float *mfcc, float *logits) {
    const int H = KWS_MFCC_COEFFS;
    stem(mfcc, A, W_stem_0_weight, B_stem_0_weight, 48, H, 63);
    dw3x3(A, B, W_ds1_depthwise_weight, B_ds1_depthwise_weight, 48, H, 63, 2, 32);
    pw1x1(B, A, W_ds1_pointwise_weight, B_ds1_pointwise_weight, 48, 96, H * 32);
    dw3x3(A, B, W_ds2_depthwise_weight, B_ds2_depthwise_weight, 96, H, 32, 1, 32);
    pw1x1(B, A, W_ds2_pointwise_weight, B_ds2_pointwise_weight, 96, 96, H * 32);
    dw3x3(A, B, W_ds3_depthwise_weight, B_ds3_depthwise_weight, 96, H, 32, 2, 16);
    pw1x1(B, A, W_ds3_pointwise_weight, B_ds3_pointwise_weight, 96, 192, H * 16);
    dw3x3(A, B, W_ds4_depthwise_weight, B_ds4_depthwise_weight, 192, H, 16, 1, 16);
    pw1x1(B, A, W_ds4_pointwise_weight, B_ds4_pointwise_weight, 192, 192, H * 16);
    float feat[192];
    for (int c = 0; c < 192; c++) {
        float s = 0; const float *p = A + c * H * 16;
        for (int i = 0; i < H * 16; i++) s += p[i];
        feat[c] = s / (H * 16);
    }
    for (int k = 0; k < KWS_NUM_CLASSES; k++) {
        float s = B_fc[k];
        for (int c = 0; c < 192; c++) s += W_fc[k * 192 + c] * feat[c];
        logits[k] = s;
    }
}
