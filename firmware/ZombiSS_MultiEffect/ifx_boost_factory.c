/* Audio hot path: build at -O2 even when the Arduino sketch uses -Os, and keep
 * GCC from turning small copy loops into memcpy/memset library calls. */
#pragma GCC optimize ("O2", "no-tree-loop-distribute-patterns")

#include "ifx_boost_factory.h"
#include <string.h>

#define BF_BARRIER() __asm volatile ("" ::: "memory")
#if defined(__arm__)
#undef  BF_BARRIER
#define BF_BARRIER() __asm volatile ("dmb sy" ::: "memory")
#endif

/* Working copies of the netlists (voice patches are applied here). */
static bf_elem s_work[BF_MAX_STAGES][BF_MAX_ELEMS + 1];

uint8_t IFX_BF_ModelCount(void) { return BF_MODEL_COUNT; }
const char *IFX_BF_ModelName(uint8_t m) {
    return m < BF_MODEL_COUNT ? BF_MODELS[m].name : "?";
}
uint8_t IFX_BF_VoiceCount(uint8_t m) {
    return m < BF_MODEL_COUNT ? BF_MODELS[m].nVoices : 1;
}
const char *IFX_BF_VoiceName(uint8_t m, uint8_t v) {
    if (m >= BF_MODEL_COUNT || v >= BF_MODELS[m].nVoices) return "-";
    return BF_MODELS[m].voice[v];
}

/* ---- op-amp output swing: quadratic soft knee (C1-continuous) ---------- */
static inline float rail(float y, float lo, float hi) {
    const float w = 0.25f;
    if (y > hi - w) {
        if (y >= hi + w) return hi;
        float d = y - (hi - w);
        return y - d * d / (4.0f * w);
    }
    if (y < lo + w) {
        if (y <= lo - w) return lo;
        float d = (lo + w) - y;
        return y + d * d / (4.0f * w);
    }
    return y;
}

/* Op-amp slew-rate limit then output-swing limit, over a block. */
static void IFX_RAMFUNC(post_block)(const bf_stage_desc *sd, float *prev, float *x, int n, float dt) {
    const bool hasSlew = sd->slew > 0.0f, hasRail = sd->railHi > sd->railLo;
    if (!hasSlew && !hasRail) return;
    const float dmax = sd->slew * dt;
    float pv = *prev;
    for (int i = 0; i < n; i++) {
        float y = x[i];
        if (hasSlew) {
            float d = y - pv;
            if (d >  dmax) d =  dmax;
            if (d < -dmax) d = -dmax;
            y = pv + d;
            pv = y;
        }
        if (hasRail) y = rail(y, sd->railLo, sd->railHi);
        x[i] = y;
    }
    *prev = pv;
}

static void netlist_for(const bf_model_desc *md, int s, bf_netlist *nl) {
    *nl = *md->st[s].nl;
    nl->elems = s_work[s];
}

/* Build a complete coefficient set on the UI core. */
static bool build_set(IFX_BoostFactory *bf, bf_coefset *cs, bool reset) {
    const uint8_t m = bf->model < BF_MODEL_COUNT ? bf->model : 0;
    const bf_model_desc *md = &BF_MODELS[m];
    bf_elem *work[BF_MAX_STAGES] = { 0 };

    for (int s = 0; s < md->nStages; s++) {
        memcpy(s_work[s], md->st[s].nl->elems, md->st[s].nl->nElems * sizeof(bf_elem));
        memset(&s_work[s][md->st[s].nl->nElems], 0, sizeof(bf_elem));
        work[s] = s_work[s];
    }
    float knobs[BF_MAX_KNOBS] = { 0.5f, 0.5f, 0.5f, 0.5f };
    bf_ui ui = { bf->drive, bf->tone, bf->voice };
    float inGain = 1.0f;
    md->map(&ui, knobs, &inGain, work);

    cs->model  = m;
    cs->inGain = inGain;
    /* stage layout: [pre (base rate)] [OS group] [post (base rate)] */
    {
        int s = 0;
        while (s < md->nStages && !md->st[s].os) s++;
        int s0 = s, s1 = s;
        while (s1 < md->nStages && md->st[s1].os) s1++;
        cs->nPre = (uint8_t)s0; cs->s0 = (uint8_t)s0; cs->s1 = (uint8_t)s1;
        cs->osf = bf->osCap ? 1 : md->osFactor;
        /* a rail/slew on the last base-rate stage feeding the OS group is
         * applied after upsampling so that clipping is oversampled */
        cs->preRail = (s0 > 0 && s0 < s1) ? (uint8_t)(s0 - 1) : 0xFF;
        cs->auxOs = cs->auxBase = 0;
        for (int k = 0; k < md->nStages; k++)
            if (md->st[k].aux) { if (md->st[k].os) cs->auxOs = 1; else cs->auxBase = 1; }
    }
    float u = 0.0f, uOsIn = 0.0f, uOsOut = 0.0f;
    bool  inOs = false;
    for (int s = 0; s < md->nStages; s++) {
        const bf_stage_desc *sd = &md->st[s];
        bf_netlist nl;
        netlist_for(md, s, &nl);
        double T = 1.0 / ((double)bf->fs * (sd->os ? cs->osf : 1));
        if (!bf_dk_build(&cs->dk[s], &nl, T, knobs)) return false;
        if (reset) {
            if (sd->os && !inOs) { inOs = true; uOsIn = u; }
            float y = 0.0f;
            if (!bf_dk_dc_init(&cs->init[s], &nl, T, knobs, u, &y)) return false;
            if (sd->railHi > sd->railLo) y = rail(y, sd->railLo, sd->railHi);
            cs->slew0[s] = y;
            if (s == cs->preRail) uOsIn = y;
            u = y;
            if (sd->os) uOsOut = y;
        }
    }
    if (reset) {
        /* pre-roll the resamplers at the DC operating point → no thump */
        bf_halfband_init(&cs->up0, 0); bf_halfband_init(&cs->up1, 1);
        bf_halfband_init(&cs->dn0, 0); bf_halfband_init(&cs->dn1, 1);
        for (int n = 0; n < 512; n++) {
            float a[2], b[2], c[2] = { uOsOut, uOsOut };
            bf_hb_up(&cs->up0, uOsIn, a);
            bf_hb_up(&cs->up1, uOsIn, b);
            bf_hb_down(&cs->dn1, c);
            bf_hb_down(&cs->dn0, c);
        }
    }
    cs->reset = reset;
    return true;
}

void IFX_BF_Init(IFX_BoostFactory *bf, float sampleRate_Hz) {
    memset(bf, 0, sizeof(*bf));
    bf->fs = sampleRate_Hz;
    bf->model = 0; bf->voice = 0;
    bf->drive = 0.5f; bf->tone = 0.5f;
    bf->levelTarget = bf->level = 1.0f;
    build_set(bf, &bf->set[0], true);
    bf->set[1] = bf->set[0];
    bf->pubIdx = 0; bf->pubSeq = bf->ackSeq = bf->seenSeq = 1;
    bf->cur = 0;
    IFX_BF_Reset(bf);
}

void IFX_BF_SetModel(IFX_BoostFactory *bf, uint8_t model) {
    if (model >= BF_MODEL_COUNT) model = BF_MODEL_COUNT - 1;
    if (model != bf->model) {
        bf->model = model;
        bf->osCap = 0;              /* every model starts at full quality */
        if (bf->voice >= BF_MODELS[model].nVoices) bf->voice = 0;
        bf->modelDirty = true;
        bf->dirty = true;
    }
}
void IFX_BF_SetVoice(IFX_BoostFactory *bf, uint8_t v) {
    if (v >= BF_MODELS[bf->model].nVoices) v = BF_MODELS[bf->model].nVoices - 1;
    if (v != bf->voice) { bf->voice = v; bf->dirty = true; }
}
void IFX_BF_SetLowCpu(IFX_BoostFactory *bf, bool low) {
    uint8_t v = low ? 1 : 0;
    if (v != bf->osCap) {
        bf->osCap = v;
        bf->modelDirty = true;     /* new sample period → re-solve operating point */
        bf->dirty = true;
    }
}
bool IFX_BF_IsLowCpu(const IFX_BoostFactory *bf) { return bf->osCap != 0; }

void IFX_BF_SetDrive(IFX_BoostFactory *bf, float d) { bf->drive = d; bf->dirty = true; }
void IFX_BF_SetTone(IFX_BoostFactory *bf, float t)  { bf->tone = t;  bf->dirty = true; }
void IFX_BF_SetLevelDb(IFX_BoostFactory *bf, float dB) {
    bf->levelTarget = powf(10.0f, dB * 0.05f);
}

bool IFX_BF_Service(IFX_BoostFactory *bf) {
    if (!bf->dirty) return false;
    if (bf->ackSeq != bf->pubSeq) return false;      /* audio hasn't switched yet */
    uint8_t w = (uint8_t)(1u - bf->pubIdx);
    bf->dirty = false;
    bool reset = bf->modelDirty;
    bf->modelDirty = false;
    if (!build_set(bf, &bf->set[w], reset)) return false;
    BF_BARRIER();
    bf->pubIdx = w;
    BF_BARRIER();
    bf->pubSeq = bf->pubSeq + 1;
    return true;
}

void IFX_BF_Reset(IFX_BoostFactory *bf) {
    bf_coefset *cs = &bf->set[bf->cur];
    memcpy(bf->st, cs->init, sizeof(bf->st));
    bf->up0 = cs->up0; bf->up1 = cs->up1; bf->dn0 = cs->dn0; bf->dn1 = cs->dn1;
    bf_halfband_init(&bf->ua0, 0); bf_halfband_init(&bf->ua1, 1);  /* aux DC = 0 */
    memcpy(bf->slew, cs->slew0, sizeof(bf->slew));
    bf->fade = 0.0f;
}

static void IFX_RAMFUNC(upsample)(bf_halfband *h0, bf_halfband *h1, int osf,
                     const float *in, float *out, int n) {
    if (osf == 4) {
        for (int i = 0; i < n; i++) {
            float a[2];
            bf_hb_up(h0, in[i], a);
            bf_hb_up(h1, a[0], out);
            bf_hb_up(h1, a[1], out + 2);
            out += 4;
        }
    } else {
        for (int i = 0; i < n; i++) { bf_hb_up(h0, in[i], out); out += 2; }
    }
}

static void IFX_RAMFUNC(downsample)(bf_halfband *h0, bf_halfband *h1, int osf,
                       const float *in, float *out, int n) {
    if (osf == 4) {
        for (int i = 0; i < n; i++) {
            float c[2];
            c[0] = bf_hb_down(h1, in);
            c[1] = bf_hb_down(h1, in + 2);
            out[i] = bf_hb_down(h0, c);
            in += 4;
        }
    } else {
        for (int i = 0; i < n; i++) { out[i] = bf_hb_down(h0, in); in += 2; }
    }
}

void IFX_RAMFUNC(IFX_BF_Process)(IFX_BoostFactory *bf, float *buf, int n) {
    if (n > BF_BLOCK_MAX) n = BF_BLOCK_MAX;
    if (bf->pubSeq != bf->seenSeq) {
        bf->seenSeq = bf->pubSeq;
        bf->cur = bf->pubIdx;
        if (bf->set[bf->cur].reset) IFX_BF_Reset(bf);
        BF_BARRIER();
        bf->ackSeq = bf->seenSeq;
    }
    const bf_coefset *cs = &bf->set[bf->cur];
    const bf_model_desc *md = &BF_MODELS[cs->model];
    const int ns = md->nStages, osf = cs->osf;
    const float dt1 = 1.0f / bf->fs, dto = dt1 / (float)osf;

    const float gin = BF_IN_FS_VOLTS * cs->inGain;
    for (int i = 0; i < n; i++) { buf[i] *= gin; bf->in1[i] = buf[i]; }

    /* base-rate stages before the oversampled group */
    for (int s = 0; s < cs->nPre; s++) {
        bf_dk_block(&cs->dk[s], &bf->st[s], buf, md->st[s].aux ? bf->in1 : 0, n);
        if (s != cs->preRail) post_block(&md->st[s], &bf->slew[s], buf, n, dt1);
    }
    /* oversampled group (or the same stages at base rate in low-CPU mode) */
    if (cs->s1 > cs->s0 && osf == 1) {
        if (cs->preRail != 0xFF)
            post_block(&md->st[cs->preRail], &bf->slew[cs->preRail], buf, n, dt1);
        for (int s = cs->s0; s < cs->s1; s++) {
            bf_dk_block(&cs->dk[s], &bf->st[s], buf, md->st[s].aux ? bf->in1 : 0, n);
            post_block(&md->st[s], &bf->slew[s], buf, n, dt1);
        }
    } else if (cs->s1 > cs->s0) {
        const int no = n * osf;
        upsample(&bf->up0, &bf->up1, osf, buf, bf->os, n);
        if (cs->auxOs) upsample(&bf->ua0, &bf->ua1, osf, bf->in1, bf->osAux, n);
        if (cs->preRail != 0xFF)
            post_block(&md->st[cs->preRail], &bf->slew[cs->preRail], bf->os, no, dto);
        for (int s = cs->s0; s < cs->s1; s++) {
            bf_dk_block(&cs->dk[s], &bf->st[s], bf->os, md->st[s].aux ? bf->osAux : 0, no);
            post_block(&md->st[s], &bf->slew[s], bf->os, no, dto);
        }
        downsample(&bf->dn0, &bf->dn1, osf, bf->os, buf, n);
    }
    /* base-rate stages after it */
    for (int s = cs->s1; s < ns; s++) {
        bf_dk_block(&cs->dk[s], &bf->st[s], buf, md->st[s].aux ? bf->in1 : 0, n);
        post_block(&md->st[s], &bf->slew[s], buf, n, dt1);
    }

    /* level smoothing + post-reset fade-in (lets coupling caps settle) */
    const float gk = md->makeup * (1.0f / BF_OUT_FS_VOLTS);
    float lvl = bf->level, tgt = bf->levelTarget, fade = bf->fade;
    bool bad = false;
    for (int i = 0; i < n; i++) {
        lvl += 0.002f * (tgt - lvl);
        float g = lvl * gk;
        if (fade < 1.0f) {
            fade += 1.0f / 1024.0f;
            float f = fade - 0.25f;
            g *= f > 0.0f ? f * (4.0f / 3.0f) : 0.0f;
        }
        float y = buf[i] * g;
        if (y != y) { bad = true; y = 0.0f; }
        buf[i] = y;
    }
    bf->level = lvl; bf->fade = fade;
    if (bad) IFX_BF_Reset(bf);
}
