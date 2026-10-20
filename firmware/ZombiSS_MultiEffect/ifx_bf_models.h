#ifndef IFX_BF_MODELS_H
#define IFX_BF_MODELS_H

/*
 * Boost Factory — pedal model descriptors.
 *
 * Every model is a chain of up to four circuit stages (netlists) cut at
 * points where the analog circuit itself is buffered (op-amp outputs) or
 * where a transistor collector can be represented by its Thevenin source.
 * Stages flagged `os` run at the oversampled rate.
 */

#include "ifx_bf_circuit.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BF_MAX_STAGES 4

typedef struct {
    const bf_netlist *nl;
    uint8_t os;          /* 1 = runs at oversampled rate                     */
    uint8_t aux;         /* 1 = u2 is the pedal input (at this stage's rate) */
    float railLo, railHi;/* op-amp output swing (V); railHi <= railLo → none */
    float slew;          /* V/s output slew limit, 0 = none                  */
} bf_stage_desc;

/* UI parameters handed to the model mapper (all 0..1 except voice) */
typedef struct { float drive, tone; uint8_t voice; } bf_ui;

typedef struct {
    const char *name;            /* ≤ 10 chars for the OLED              */
    uint8_t nStages;
    bf_stage_desc st[BF_MAX_STAGES];
    uint8_t osFactor;            /* 2 or 4                               */
    uint8_t nVoices;
    const char *voice[4];
    const char *driveLabel, *toneLabel;
    float makeup;                /* output volts → nominal level         */
    float defDrive, defTone;     /* knob positions loaded on model select */
    /* maps UI → pot rotations + digital input gain; patches netlist copies
     * for the selected voice (diodes, caps, pickup) */
    void (*map)(const bf_ui *ui, float *knobs, float *inGain,
                bf_elem *work[BF_MAX_STAGES]);
} bf_model_desc;

#define BF_MODEL_COUNT 9
extern const bf_model_desc BF_MODELS[BF_MODEL_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* IFX_BF_MODELS_H */
