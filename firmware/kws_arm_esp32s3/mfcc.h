#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define KWS_SAMPLE_RATE 16000
#define KWS_CLIP_SAMPLES 16000          /* 1.0 s, same as the notebook */
#define KWS_MFCC_COEFFS 13
#define KWS_MFCC_FRAMES 63              /* 1 + 16000/256 (librosa center=True) */
/* pcm: KWS_CLIP_SAMPLES floats in [-1,1].  out: [13][63] row-major (coef, frame),
 * already normalised (x-mean)/(std+1e-6) over the whole matrix, exactly like the notebook. */
void mfcc_compute(const float *pcm, float *out);

#ifdef __cplusplus
}
#endif
