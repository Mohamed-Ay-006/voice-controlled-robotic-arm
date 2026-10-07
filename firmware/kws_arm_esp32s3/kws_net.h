#pragma once
#ifdef __cplusplus
extern "C" {
#endif
/* work_a / work_b: two float buffers of KWS_NET_WORK_FLOATS each (put them in PSRAM on the S3). */
#define KWS_NET_WORK_FLOATS 39936
#define KWS_MAX_CLASSES 16
void kws_net_set_work(float *work_a, float *work_b);
int kws_num_classes(void);
const char *kws_label(int i);
/* mfcc: [13][63] float. logits: kws_num_classes() floats. */
void kws_net_run(const float *mfcc, float *logits);

#ifdef __cplusplus
}
#endif
