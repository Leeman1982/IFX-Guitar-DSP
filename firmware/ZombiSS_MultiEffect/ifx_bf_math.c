#include "ifx_bf_math.h"
#include <string.h>

/* Half-band coefficient sets — designed with the Valenzuela/Constantinides
 * elliptic polyphase method (as in L. de Soras' HIIR) and verified offline. */
static const float BF_HB_A[5] = { 0.0610594761f, 0.2209691851f, 0.4303240313f,
                                  0.6508354670f, 0.8765982613f };
static const float BF_HB_B[2] = { 0.1270102653f, 0.5664591041f };

void bf_halfband_init(bf_halfband *h, int stageB) {
    memset(h, 0, sizeof(*h));
    h->stageB = (uint8_t)(stageB != 0);
    const float *c = stageB ? BF_HB_B : BF_HB_A;
    int n = stageB ? 2 : 5;
    for (int i = 0; i < n; i++) h->c[i] = c[i];   /* even → branch 0, odd → 1 */
}
