#include "effect_chain.h"
#include "pico/platform.h"

/* Compile-time guard: EQ param count must match the actual number of EQ bands */
_Static_assert(EQ_PARAM_COUNT == EQ10_BANDS,
    "EQ_PARAM_COUNT in effect_chain.h must equal EQ10_BANDS in ifx_10band_eq.h");

static void init_param_descs(EffectChain *ec) {

    /* ===== TS Boost ===== */
    ec->fxNames[FX_TSBOOST] = "TS BOOST";
    ec->paramCount[FX_TSBOOST] = TS_PARAM_COUNT;
    ec->params[FX_TSBOOST][TS_DRIVE] = (ParamDesc){"Drive", 1.0f,  100.0f, 1.0f,  10.0f, ""};
    ec->params[FX_TSBOOST][TS_TONE]  = (ParamDesc){"Tone",  0.0f,  1.0f,   0.05f, 0.5f,  ""};
    ec->params[FX_TSBOOST][TS_LEVEL] = (ParamDesc){"Level", 0.0f,  2.0f,   0.05f, 0.8f,  ""};

    /* ===== Noise Gate ===== */
    ec->fxNames[FX_NOISEGATE] = "GATE";
    ec->paramCount[FX_NOISEGATE] = NG_PARAM_COUNT;
    ec->params[FX_NOISEGATE][NG_THRESHOLD] = (ParamDesc){"Threshold", 0.01f, 0.5f,   0.01f, 0.1f,  ""};
    ec->params[FX_NOISEGATE][NG_ATTACK]    = (ParamDesc){"Attack",    0.5f,  50.0f,  0.5f,  2.0f,  "ms"};
    ec->params[FX_NOISEGATE][NG_RELEASE]   = (ParamDesc){"Release",   0.5f,  50.0f,  0.5f,  2.0f,  "ms"};
    ec->params[FX_NOISEGATE][NG_HOLD]      = (ParamDesc){"Hold",      1.0f,  200.0f, 1.0f,  20.0f, "ms"};

    /* ===== Overdrive ===== */
    ec->fxNames[FX_OVERDRIVE] = "DRIVE";
    ec->paramCount[FX_OVERDRIVE] = OD_PARAM_COUNT;
    ec->params[FX_OVERDRIVE][OD_GAIN]       = (ParamDesc){"Gain",    1.0f,   200.0f,  1.0f,  40.0f,  ""};
    ec->params[FX_OVERDRIVE][OD_BOOST]      = (ParamDesc){"Boost",   0.0f,   100.0f,  1.0f,  0.0f,   ""};
    ec->params[FX_OVERDRIVE][OD_HPF_CUTOFF] = (ParamDesc){"HPF",     50.0f,  2000.0f, 10.0f, 150.0f, "Hz"};
    ec->params[FX_OVERDRIVE][OD_LPF_CUTOFF] = (ParamDesc){"LPF",     500.0f, 20000.0f,100.0f,5000.0f,"Hz"};
    ec->params[FX_OVERDRIVE][OD_LPF_DAMP]   = (ParamDesc){"Damping", 0.1f,   2.0f,    0.05f, 1.0f,   ""};
    ec->params[FX_OVERDRIVE][OD_Q_CLIP]     = (ParamDesc){"Clip Q",  -0.5f, -0.01f,   0.01f,-0.2f,   ""};

    /* ===== 10-Band EQ (Metallica preset defaults) ===== */
    ec->fxNames[FX_EQ] = "10B EQ";
    ec->paramCount[FX_EQ] = EQ_PARAM_COUNT;
    ec->params[FX_EQ][EQ_31HZ]  = (ParamDesc){"31 Hz",  -15.0f, 15.0f, 0.5f,  6.0f, "dB"};
    ec->params[FX_EQ][EQ_63HZ]  = (ParamDesc){"63 Hz",  -15.0f, 15.0f, 0.5f,  5.0f, "dB"};
    ec->params[FX_EQ][EQ_125HZ] = (ParamDesc){"125 Hz", -15.0f, 15.0f, 0.5f,  3.0f, "dB"};
    ec->params[FX_EQ][EQ_250HZ] = (ParamDesc){"250 Hz", -15.0f, 15.0f, 0.5f, -2.0f, "dB"};
    ec->params[FX_EQ][EQ_500HZ] = (ParamDesc){"500 Hz", -15.0f, 15.0f, 0.5f, -4.0f, "dB"};
    ec->params[FX_EQ][EQ_1KHZ]  = (ParamDesc){"1k Hz",  -15.0f, 15.0f, 0.5f, -6.0f, "dB"};
    ec->params[FX_EQ][EQ_2KHZ]  = (ParamDesc){"2k Hz",  -15.0f, 15.0f, 0.5f, -5.0f, "dB"};
    ec->params[FX_EQ][EQ_4KHZ]  = (ParamDesc){"4k Hz",  -15.0f, 15.0f, 0.5f, -3.0f, "dB"};
    ec->params[FX_EQ][EQ_8KHZ]  = (ParamDesc){"8k Hz",  -15.0f, 15.0f, 0.5f,  4.0f, "dB"};
    ec->params[FX_EQ][EQ_16KHZ] = (ParamDesc){"16k Hz", -15.0f, 15.0f, 0.5f,  3.0f, "dB"};

    /* ===== Chorus ===== */
    ec->fxNames[FX_CHORUS] = "CHORUS";
    ec->paramCount[FX_CHORUS] = CH_PARAM_COUNT;
    ec->params[FX_CHORUS][CH_DELAY_A] = (ParamDesc){"Delay A",  1.0f,  40.0f,  0.5f,  10.0f, "ms"};
    ec->params[FX_CHORUS][CH_DELAY_B] = (ParamDesc){"Delay B",  1.0f,  40.0f,  0.5f,  15.0f, "ms"};
    ec->params[FX_CHORUS][CH_DEPTH_A] = (ParamDesc){"Depth A",  0.0f,  20.0f,  0.5f,  10.0f, ""};
    ec->params[FX_CHORUS][CH_DEPTH_B] = (ParamDesc){"Depth B",  0.0f,  20.0f,  0.5f,  10.0f, ""};
    ec->params[FX_CHORUS][CH_RATE_A]  = (ParamDesc){"Rate A",   0.1f,  10.0f,  0.1f,  1.0f,  "Hz"};
    ec->params[FX_CHORUS][CH_RATE_B]  = (ParamDesc){"Rate B",   0.1f,  10.0f,  0.1f,  1.3f,  "Hz"};
    ec->params[FX_CHORUS][CH_GAIN_A]  = (ParamDesc){"Gain A",   0.0f,  1.0f,   0.05f, 0.25f, ""};
    ec->params[FX_CHORUS][CH_GAIN_B]  = (ParamDesc){"Gain B",   0.0f,  1.0f,   0.05f, 0.25f, ""};
    ec->params[FX_CHORUS][CH_MIX]     = (ParamDesc){"Mix",      0.0f,  1.0f,   0.05f, 0.5f,  ""};

    /* ===== Delay ===== */
    ec->fxNames[FX_DELAY] = "DELAY";
    ec->paramCount[FX_DELAY] = DL_PARAM_COUNT;
    ec->params[FX_DELAY][DL_TIME]     = (ParamDesc){"Time",     10.0f, 660.0f, 5.0f,  300.0f, "ms"};
    ec->params[FX_DELAY][DL_MIX]      = (ParamDesc){"Mix",      0.0f,  1.0f,   0.05f, 0.35f,  ""};
    ec->params[FX_DELAY][DL_FEEDBACK] = (ParamDesc){"Feedback", 0.0f,  0.95f,  0.05f, 0.4f,   ""};
}

void EffectChain_Init(EffectChain *ec) {
    init_param_descs(ec);

    ec->active[FX_TSBOOST]   = false;
    ec->active[FX_NOISEGATE] = false;
    ec->active[FX_OVERDRIVE] = true;
    ec->active[FX_EQ]        = true;   /* EQ on by default — Metallica preset */
    ec->active[FX_CHORUS]    = false;
    ec->active[FX_DELAY]     = false;
    ec->masterVolume = 0.8f;
    ec->volSmoothed  = 0.8f;

    IFX_TSBoost_Init(&ec->tsboost, SAMPLE_RATE_HZ);
    IFX_TSBoost_SetDrive(&ec->tsboost, ec->params[FX_TSBOOST][TS_DRIVE].value);
    IFX_TSBoost_SetTone(&ec->tsboost,  ec->params[FX_TSBOOST][TS_TONE].value);
    IFX_TSBoost_SetLevel(&ec->tsboost, ec->params[FX_TSBOOST][TS_LEVEL].value);

    IFX_NoiseGate_Init(&ec->noiseGate,
        ec->params[FX_NOISEGATE][NG_THRESHOLD].value,
        ec->params[FX_NOISEGATE][NG_ATTACK].value,
        ec->params[FX_NOISEGATE][NG_RELEASE].value,
        ec->params[FX_NOISEGATE][NG_HOLD].value,
        SAMPLE_RATE_HZ);

    IFX_Overdrive_Init(&ec->overdrive, SAMPLE_RATE_HZ,
        ec->params[FX_OVERDRIVE][OD_HPF_CUTOFF].value,
        ec->params[FX_OVERDRIVE][OD_GAIN].value,
        ec->params[FX_OVERDRIVE][OD_LPF_CUTOFF].value,
        ec->params[FX_OVERDRIVE][OD_LPF_DAMP].value);
    IFX_Overdrive_SetQ(&ec->overdrive, ec->params[FX_OVERDRIVE][OD_Q_CLIP].value);

    IFX_10BandEQ_Init(&ec->eq, SAMPLE_RATE_HZ);
    /* Init already loads IFX_EQ10_METALLICA_PRESET; param defaults above
     * match that preset exactly, so no further SetBand calls are needed. */

    IFX_Chorus_Init(&ec->chorus,
        ec->params[FX_CHORUS][CH_DELAY_A].value,
        ec->params[FX_CHORUS][CH_DELAY_B].value,
        ec->params[FX_CHORUS][CH_DEPTH_A].value,
        ec->params[FX_CHORUS][CH_DEPTH_B].value,
        ec->params[FX_CHORUS][CH_GAIN_A].value,
        ec->params[FX_CHORUS][CH_GAIN_B].value,
        ec->params[FX_CHORUS][CH_RATE_A].value,
        ec->params[FX_CHORUS][CH_RATE_B].value,
        ec->params[FX_CHORUS][CH_MIX].value,
        SAMPLE_RATE_HZ);

    IFX_Delay_Init(&ec->delay,
        ec->params[FX_DELAY][DL_TIME].value,
        ec->params[FX_DELAY][DL_MIX].value,
        ec->params[FX_DELAY][DL_FEEDBACK].value,
        SAMPLE_RATE_HZ);
}

float __not_in_flash_func(EffectChain_Process)(EffectChain *ec, float inp) {
    float sig = inp;
    if (ec->active[FX_TSBOOST])   sig = IFX_TSBoost_Update(&ec->tsboost, sig);
    if (ec->active[FX_NOISEGATE]) sig = IFX_NoiseGate_Update(&ec->noiseGate, sig);
    if (ec->active[FX_OVERDRIVE]) sig = IFX_Overdrive_Update(&ec->overdrive, sig);
    if (ec->active[FX_EQ])        sig = IFX_10BandEQ_Update(&ec->eq, sig);
    if (ec->active[FX_CHORUS])    sig = IFX_Chorus_Update(&ec->chorus, sig);
    if (ec->active[FX_DELAY])     sig = IFX_Delay_Update(&ec->delay, sig);

    /* One-pole ramp toward the UI's target volume (~14 ms time constant)
     * so knob turns never step the gain hard enough to click. */
    ec->volSmoothed += 0.0015f * (ec->masterVolume - ec->volSmoothed);
    sig *= ec->volSmoothed;
    if (sig >  1.0f) sig =  1.0f;
    if (sig < -1.0f) sig = -1.0f;
    return sig;
}

void EffectChain_ToggleEffect(EffectChain *ec, uint8_t fxIndex) {
    if (fxIndex < FX_COUNT) ec->active[fxIndex] = !ec->active[fxIndex];
}

void EffectChain_SetParam(EffectChain *ec, uint8_t fxIndex, uint8_t paramIndex, float value) {
    if (fxIndex >= FX_COUNT) return;
    if (paramIndex >= ec->paramCount[fxIndex]) return;

    ParamDesc *p = &ec->params[fxIndex][paramIndex];
    if (value < p->min) value = p->min;
    if (value > p->max) value = p->max;
    p->value = value;

    switch (fxIndex) {
    case FX_TSBOOST:
        switch (paramIndex) {
        case TS_DRIVE: IFX_TSBoost_SetDrive(&ec->tsboost, value); break;
        case TS_TONE:  IFX_TSBoost_SetTone(&ec->tsboost, value);  break;
        case TS_LEVEL: IFX_TSBoost_SetLevel(&ec->tsboost, value); break;
        } break;

    case FX_NOISEGATE:
        switch (paramIndex) {
        case NG_THRESHOLD: IFX_NoiseGate_SetThreshold(&ec->noiseGate, value); break;
        case NG_ATTACK: case NG_RELEASE:
            IFX_NoiseGate_SetAttackRelease(&ec->noiseGate,
                ec->params[FX_NOISEGATE][NG_ATTACK].value,
                ec->params[FX_NOISEGATE][NG_RELEASE].value,
                SAMPLE_RATE_HZ); break;
        case NG_HOLD: IFX_NoiseGate_SetHoldTime(&ec->noiseGate, value); break;
        } break;

    case FX_OVERDRIVE:
        switch (paramIndex) {
        case OD_GAIN:      IFX_Overdrive_SetGain(&ec->overdrive, value); break;
        case OD_BOOST:     IFX_Overdrive_SetBoost(&ec->overdrive, value); break;
        case OD_HPF_CUTOFF:IFX_Overdrive_SetHPF(&ec->overdrive, value); break;
        case OD_LPF_CUTOFF: case OD_LPF_DAMP:
            IFX_Overdrive_SetLPF(&ec->overdrive,
                ec->params[FX_OVERDRIVE][OD_LPF_CUTOFF].value,
                ec->params[FX_OVERDRIVE][OD_LPF_DAMP].value); break;
        case OD_Q_CLIP:    IFX_Overdrive_SetQ(&ec->overdrive, value); break;
        } break;

    case FX_EQ:
        /* paramIndex == band index (0..9) */
        IFX_10BandEQ_SetBand(&ec->eq, paramIndex, value);
        break;

    case FX_CHORUS:
        switch (paramIndex) {
        case CH_DELAY_A: case CH_DELAY_B:
            IFX_Chorus_SetDelayTime(&ec->chorus,
                ec->params[FX_CHORUS][CH_DELAY_A].value,
                ec->params[FX_CHORUS][CH_DELAY_B].value); break;
        case CH_DEPTH_A: case CH_DEPTH_B:
            IFX_Chorus_SetDepth(&ec->chorus,
                ec->params[FX_CHORUS][CH_DEPTH_A].value,
                ec->params[FX_CHORUS][CH_DEPTH_B].value); break;
        case CH_RATE_A: case CH_RATE_B:
            IFX_Chorus_SetRate(&ec->chorus,
                ec->params[FX_CHORUS][CH_RATE_A].value,
                ec->params[FX_CHORUS][CH_RATE_B].value); break;
        case CH_GAIN_A: case CH_GAIN_B:
            IFX_Chorus_SetGain(&ec->chorus,
                ec->params[FX_CHORUS][CH_GAIN_A].value,
                ec->params[FX_CHORUS][CH_GAIN_B].value); break;
        case CH_MIX: IFX_Chorus_SetMix(&ec->chorus, value); break;
        } break;

    case FX_DELAY:
        switch (paramIndex) {
        case DL_TIME:     IFX_Delay_SetLength(&ec->delay, value, SAMPLE_RATE_HZ); break;
        case DL_MIX:      IFX_Delay_SetMix(&ec->delay, value); break;
        case DL_FEEDBACK: IFX_Delay_SetFeedback(&ec->delay, value); break;
        } break;
    }
}

void EffectChain_SetMasterVolume(EffectChain *ec, float vol) {
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    ec->masterVolume = vol;
}
