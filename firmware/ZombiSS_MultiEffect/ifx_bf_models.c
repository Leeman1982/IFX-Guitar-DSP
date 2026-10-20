/*
 * Boost Factory — component-level netlists.
 *
 * Component values are transcribed from published schematics of the
 * original pedals (cross-checked against the Aion FX build documents for
 * the TS-808/Stratus, RAT/Helios, Klon/Refractor, Distortion+/Aphelion,
 * DS-1/Comet, Big Muff/Halo, Fuzz Face/Proteus and Rangemaster/Radian).
 * Designators in comments match those schematics.
 *
 * Conventions
 *   node 0 = ground.  u[0] = stage input (volts), u[1] = 1.0 for supplies.
 *   Pots: R(pin1–wiper) = total·taper(knob); knob 1 = fully clockwise.
 *   Knob slots: 0 = Drive, 1 = Tone, 2 = spare.
 */
#include "ifx_bf_models.h"
#include <stddef.h>

#define R_(a,b,v)        { BF_E_R,     a, b, 0, 0, 0, (float)(v) }
#define C_(a,b,v)        { BF_E_C,     a, b, 0, 0, 0, (float)(v) }
#define CJ_(a,b,v)       { BF_E_C,     a, b, 0, 0, 1, (float)(v) }   /* junction parasitic, BE */
#define L_(a,b,v)        { BF_E_L,     a, b, 0, 0, 0, (float)(v) }
#define V_(p,m,src,v)    { BF_E_V,     p, m, 0, src, 0, (float)(v) }
#define POT_(p1,w,p3,R,k,t) { BF_E_POT, p1, w, p3, k, t, (float)(R) }
#define OPA_(p,m,o,gbw)  { BF_E_OPAMP, p, m, o, 0, 0, (float)(gbw) }
#define BUF_(i,o)        { BF_E_BUF,   i, 0, o, 0, 0, 0.0f }
#define DP_(a,k,df,nf,dr,nr) { BF_E_DPAIR, a, k, dr, df, nf, (float)(nr) }
#define Q_(c,b,e,m)      { BF_E_BJT,   c, b, e, m, 0, 0.0f }
#define QFA_(c,b,e,m)    { BF_E_BJTFA, c, b, e, m, 0, 0.0f }
#define END_             { 0, 0, 0, 0, 0, 0, 0.0f }

#define NL(arr, nodes, op, on) { arr, (uint8_t)(sizeof(arr)/sizeof(arr[0]) - 1), nodes, op, on }

enum { K_DRIVE = 0, K_TONE = 1, K_AUX = 2 };
enum { LIN = BF_TAPER_LIN, LOGA = BF_TAPER_A, REVC = BF_TAPER_C };

static bf_elem *find(bf_elem *e, uint8_t kind, int nth) {
    for (; e && e->kind; e++)
        if (e->kind == kind && nth-- == 0) return e;
    return NULL;
}

static void set_pair(bf_elem *d, uint8_t model, uint8_t nf, uint8_t nr) {
    if (!d) return;
    d->aux = model; d->aux2 = nf;          /* forward branch */
    d->n3  = model; d->value = (float)nr;  /* reverse branch */
}

/* ===========================================================================
 * 1. Ibanez TS-808 Tube Screamer
 * ======================================================================== */
/* Clipping amp IC1A (JRC4558D, GBW ≈ 3 MHz) with D1/D2 in the feedback loop. */
static const bf_elem TS_CLIP[] = {
    V_(1, 0, 0, 1.0),               /* input (buffered guitar)              */
    V_(2, 0, 1, 4.5),               /* VB                                   */
    C_(1, 3, 1e-6),                 /* C2  1 µF                             */
    R_(3, 2, 10e3),                 /* R4  10 k bias                        */
    OPA_(3, 4, 5, 3e6),             /* IC1A                                 */
    C_(4, 6, 47e-9),                /* C4  47 n                             */
    R_(6, 2, 4.7e3),                /* R5  4k7  → 720 Hz shelf              */
    R_(4, 7, 51e3),                 /* R6  51 k                             */
    POT_(5, 7, 7, 500e3, K_DRIVE, LOGA), /* DRIVE 500 kA (rheostat)         */
    C_(4, 5, 51e-12),               /* C3  51 p                             */
    DP_(4, 5, BF_D_1N914, 1, BF_D_1N914, 1),  /* D1/D2 1N914                */
    END_
};
/* Tone stage IC1B + output: R7/C5 723 Hz LPF, TONE 20k with C6/R9 shunt. */
static const bf_elem TS_TONE[] = {
    V_(1, 0, 0, 1.0),
    V_(2, 0, 1, 4.5),
    R_(1, 3, 1e3),                  /* R7  1 k                              */
    C_(3, 0, 220e-9),               /* C5  220 n                            */
    R_(3, 2, 10e3),                 /* R8  10 k                             */
    OPA_(3, 4, 7, 0),               /* IC1B (gain ≤ 5.5 → GBW irrelevant)   */
    POT_(3, 5, 4, 20e3, K_TONE, LIN),   /* TONE 20 k: pin1 = +in, pin3 = −in */
    C_(5, 6, 220e-9),               /* C6  220 n                            */
    R_(6, 0, 220),                  /* R9  220 Ω                            */
    R_(4, 7, 1e3),                  /* R10 1 k                              */
    C_(7, 8, 1e-6),                 /* C7  1 µF                             */
    R_(8, 9, 1e3),                  /* R11 1 k                              */
    R_(9, 2, 100e3),                /* LEVEL 100 kA (full, as load)         */
    END_
};
static const bf_netlist TS_CLIP_NL = NL(TS_CLIP, 8, 5, 0);
static const bf_netlist TS_TONE_NL = NL(TS_TONE, 10, 9, 2);

static void ts_map(const bf_ui *ui, float *k, float *g, bf_elem *w[]) {
    k[K_DRIVE] = ui->drive;
    k[K_TONE]  = ui->tone;
    *g = 1.0f;
    bf_elem *d = find(w[0], BF_E_DPAIR, 0);
    switch (ui->voice) {
    case 1:  set_pair(d, BF_D_1N914, 1, 2); break;     /* asymmetric 1:2    */
    case 2:  set_pair(d, BF_D_1N914, 2, 2); break;     /* 2+2 Si            */
    case 3:  set_pair(d, BF_D_LED_RED, 1, 1); break;   /* LEDs              */
    default: set_pair(d, BF_D_1N914, 1, 1); break;     /* stock             */
    }
}

/* ===========================================================================
 * 2. Klon Centaur
 * ======================================================================== */
/* Stage A (base rate, linear): input buffer IC1A → gain stage IC1B.
 * The diode network can't load IC1B's inputs (its output is a voltage
 * source), so this section is computed separately. */
static const bf_elem KLON_GAIN[] = {
    V_(2, 0, 1, 4.5),               /* VR                                   */
    V_(1, 2, 0, 1.0),               /* input riding on VR                   */
    BUF_(1, 3),                     /* IC1A input buffer                    */
    C_(3, 4, 100e-9),               /* C3                                   */
    R_(4, 5, 10e3),                 /* R6                                   */
    C_(4, 5, 68e-9),                /* C5                                   */
    POT_(5, 2, 8, 100e3, K_DRIVE, LIN),   /* GAIN A                         */
    R_(8, 9, 2e3),                  /* R10                                  */
    R_(9, 10, 15e3),                /* R11                                  */
    C_(9, 10, 82e-9),               /* C7                                   */
    OPA_(5, 10, 11, 3e6),           /* IC1B TL072 (gain up to ×212)         */
    R_(10, 11, 422e3),              /* R12                                  */
    C_(10, 11, 390e-12),            /* C8                                   */
    END_
};
/* Stage B (oversampled): clipper, GAIN B blend network, clean path and the
 * IC2A summing amp.  u0 = IC1B output, u2 = pedal input (IC1A output). */
static const bf_elem KLON_MAIN[] = {
    V_(2, 0, 1, 4.5),               /* VR                                   */
    V_(1, 2, 2, 1.0),               /* pedal input riding on VR             */
    BUF_(1, 3),                     /* IC1A output                          */
    V_(11, 0, 0, 1.0),              /* IC1B output                          */
    R_(3, 6, 5.1e3),                /* R5                                   */
    C_(3, 6, 68e-9),                /* C4                                   */
    R_(6, 2, 1.5e3),                /* R8                                   */
    C_(6, 7, 390e-9),               /* C6                                   */
    R_(7, 2, 1e3),                  /* R9                                   */
    POT_(6, 16, 2, 100e3, K_DRIVE, LIN),  /* GAIN B (summing-ground blend)  */
    C_(11, 12, 1e-6),               /* C9                                   */
    R_(12, 13, 1e3),                /* R13                                  */
    DP_(13, 0, BF_D_1N34A, 1, BF_D_1N34A, 1),  /* D1/D2 germanium           */
    C_(13, 14, 1e-6),               /* C10                                  */
    R_(14, 15, 47e3),               /* R16                                  */
    C_(14, 18, 2.2e-9),             /* C11                                  */
    R_(18, 16, 22e3),               /* R15                                  */
    R_(3, 17, 1.5e3),               /* R7  clean path                       */
    C_(17, 0, 1e-6),                /* C16                                  */
    R_(17, 15, 15e3),               /* R19                                  */
    R_(15, 16, 27e3),               /* R17                                  */
    C_(15, 19, 27e-9),              /* C12                                  */
    R_(19, 16, 12e3),               /* R18                                  */
    OPA_(2, 15, 20, 0),             /* IC2A summing amp (low noise gain)    */
    R_(15, 20, 392e3),              /* R20                                  */
    C_(15, 20, 820e-12),            /* C13                                  */
    END_
};
static const bf_elem KLON_TONE[] = {
    V_(1, 0, 0, 1.0),
    V_(2, 0, 1, 4.5),
    R_(1, 3, 100e3),                /* R22                                  */
    OPA_(2, 3, 4, 0),               /* IC2B (unity-gain inverter)           */
    R_(3, 4, 100e3),                /* R24                                  */
    R_(1, 5, 1.8e3),                /* R21                                  */
    POT_(7, 6, 5, 10e3, K_TONE, LIN),  /* TREBLE 10 kB                      */
    C_(6, 3, 3.9e-9),               /* C14                                  */
    R_(7, 4, 4.7e3),                /* R23                                  */
    C_(4, 8, 4.7e-6),               /* C15                                  */
    R_(8, 9, 560),                  /* R25                                  */
    R_(9, 0, 10e3),                 /* OUTPUT 10 kB (full, as load)         */
    END_
};
static const bf_netlist KLON_GAIN_NL = NL(KLON_GAIN, 12, 11, 0);
static const bf_netlist KLON_MAIN_NL = NL(KLON_MAIN, 21, 20, 2);
static const bf_netlist KLON_TONE_NL = NL(KLON_TONE, 10, 9, 0);

static void klon_map(const bf_ui *ui, float *k, float *g, bf_elem *w[]) {
    k[K_DRIVE] = ui->drive; k[K_TONE] = ui->tone; *g = 1.0f;
    bf_elem *d = find(w[1], BF_E_DPAIR, 0);
    if (ui->voice == 1) set_pair(d, BF_D_1N914, 1, 1);
    else                set_pair(d, BF_D_1N34A, 1, 1);
}

/* ===========================================================================
 * 3. MXR Distortion+ / DOD 250
 * ======================================================================== */
static const bf_elem DP_GAIN[] = {
    V_(1, 0, 0, 1.0),
    V_(2, 0, 1, 4.5),
    C_(1, 3, 10e-9),                /* C4  10 n                             */
    R_(3, 4, 10e3),                 /* R1                                   */
    R_(4, 2, 1e6),                  /* R2                                   */
    OPA_(4, 5, 6, 1e6),             /* IC1 µA741                            */
    R_(5, 6, 1e6),                  /* R3                                   */
    C_(5, 6, 10e-12),               /* C5                                   */
    C_(5, 7, 47e-9),                /* C6                                   */
    R_(7, 8, 4.7e3),                /* R4                                   */
    POT_(0, 0, 8, 1e6, K_DRIVE, REVC), /* DISTORTION 1 MC (rheostat)        */
    END_
};
static const bf_elem DP_CLIP[] = {
    V_(1, 0, 0, 1.0),
    C_(1, 2, 1e-6),                 /* C7                                   */
    R_(2, 3, 10e3),                 /* R5                                   */
    C_(3, 0, 1e-9),                 /* C8                                   */
    DP_(3, 0, BF_D_1N34A, 1, BF_D_1N34A, 1),  /* D2/D3 germanium            */
    R_(3, 0, 50e3),                 /* OUTPUT pot as load                   */
    END_
};
static const bf_netlist DP_GAIN_NL = NL(DP_GAIN, 9, 6, 0);
static const bf_netlist DP_CLIP_NL = NL(DP_CLIP, 4, 3, 0);

static void dplus_map(const bf_ui *ui, float *k, float *g, bf_elem *w[]) {
    k[K_DRIVE] = ui->drive; k[K_TONE] = ui->tone; *g = 1.0f;
    bf_elem *d = find(w[1], BF_E_DPAIR, 0);
    switch (ui->voice) {
    case 1:  set_pair(d, BF_D_1N914, 1, 1); break;     /* DOD 250            */
    case 2:  set_pair(d, BF_D_LED_RED, 1, 1); break;
    default: set_pair(d, BF_D_1N34A, 1, 1); break;     /* MXR germanium      */
    }
}

/* ===========================================================================
 * 4. ProCo RAT
 * ======================================================================== */
static const bf_elem RAT_GAIN[] = {
    V_(1, 0, 0, 1.0),
    V_(2, 0, 1, 4.5),
    C_(1, 3, 22e-9),                /* C1                                   */
    R_(3, 2, 1e6),                  /* R1                                   */
    R_(3, 4, 1e3),                  /* R2                                   */
    C_(4, 0, 1e-9),                 /* C2                                   */
    OPA_(4, 5, 6, 1e6),             /* IC1 LM308 (30 pF comp.)              */
    POT_(6, 5, 5, 100e3, K_DRIVE, LOGA),  /* DISTORTION 100 kA (rheostat)   */
    C_(5, 6, 100e-12),              /* C4                                   */
    R_(5, 7, 47),                   /* R3                                   */
    C_(7, 0, 2.2e-6),               /* C5                                   */
    R_(5, 8, 560),                  /* R4                                   */
    C_(8, 0, 4.7e-6),               /* C6                                   */
    END_
};
static const bf_elem RAT_CLIP[] = {
    V_(1, 0, 0, 1.0),
    C_(1, 2, 4.7e-6),               /* C7                                   */
    R_(2, 3, 1e3),                  /* R5                                   */
    DP_(3, 0, BF_D_1N914, 1, BF_D_1N914, 1),  /* D2/D3                      */
    POT_(3, 4, 4, 100e3, K_TONE, LOGA),  /* FILTER 100 kA (rheostat)        */
    R_(4, 5, 1.5e3),                /* R6                                   */
    C_(5, 0, 3.3e-9),               /* C8                                   */
    C_(5, 6, 22e-9),                /* C9                                   */
    R_(6, 0, 1e6),                  /* R7 (JFET gate)                       */
    END_
};
static const bf_netlist RAT_GAIN_NL = NL(RAT_GAIN, 9, 6, 0);
static const bf_netlist RAT_CLIP_NL = NL(RAT_CLIP, 7, 6, 0);

static void rat_map(const bf_ui *ui, float *k, float *g, bf_elem *w[]) {
    k[K_DRIVE] = ui->drive;
    k[K_TONE]  = 1.0f - ui->tone;          /* FILTER: clockwise = darker */
    *g = 1.0f;
    bf_elem *d = find(w[1], BF_E_DPAIR, 0);
    switch (ui->voice) {
    case 1:  set_pair(d, BF_D_LED_RED, 1, 1); break;   /* Turbo RAT          */
    case 2:  set_pair(d, BF_D_1N34A, 1, 1); break;     /* germanium mod      */
    default: set_pair(d, BF_D_1N914, 1, 1); break;
    }
}

/* ===========================================================================
 * 5. Boss DS-1
 * ======================================================================== */
static const bf_elem DS_BOOST[] = {
    V_(1, 0, 0, 1.0),
    V_(2, 0, 1, 9.0),               /* VA                                   */
    V_(3, 0, 1, 4.5),               /* VB                                   */
    R_(1, 9, 1e3),                  /* R1                                   */
    C_(9, 4, 47e-9),                /* C1                                   */
    R_(4, 3, 470e3),                /* R2                                   */
    BUF_(4, 5),                     /* Q1 emitter follower                  */
    C_(5, 6, 47e-9),                /* C2                                   */
    R_(6, 0, 100e3),                /* R4                                   */
    R_(6, 7, 470e3),                /* R5                                   */
    C_(6, 7, 250e-12),              /* C3                                   */
    R_(7, 2, 10e3),                 /* R6                                   */
    Q_(7, 6, 8, BF_Q_2N5088),       /* Q2 booster                           */
    R_(8, 0, 22),                   /* R7                                   */
    END_
};
static const bf_elem DS_GAIN[] = {
    V_(1, 0, 0, 1.0),
    V_(4, 0, 1, 4.5),
    R_(1, 2, 10e3),                 /* Q2 collector source (R6)             */
    C_(2, 3, 470e-9),               /* C5                                   */
    R_(3, 4, 100e3),                /* R8                                   */
    R_(3, 5, 47e3),                 /* R9                                   */
    BUF_(5, 10),                    /* IC1B follower                        */
    OPA_(10, 9, 6, 0),              /* IC1A (gain ≤ 22, ~1 MHz GBW → >45 kHz) */
    R_(9, 7, 100e3),                /* R10                                  */
    POT_(6, 7, 11, 100e3, K_DRIVE, LIN),  /* DIST 100 kB                    */
    C_(6, 7, 250e-12),              /* C6                                   */
    R_(11, 8, 4.7e3),               /* R11                                  */
    C_(8, 0, 1e-6),                 /* C7                                   */
    END_
};
static const bf_elem DS_CLIP[] = {
    V_(1, 0, 0, 1.0),
    V_(2, 0, 1, 4.5),
    R_(1, 3, 2.2e3),                /* R12                                  */
    C_(3, 4, 470e-9),               /* C8                                   */
    DP_(4, 2, BF_D_1N914, 1, BF_D_1N914, 1),  /* D5/D6 to VB                */
    C_(4, 2, 10e-9),                /* C9                                   */
    R_(4, 5, 6.8e3),                /* R13                                  */
    C_(5, 2, 100e-9),               /* C11                                  */
    C_(4, 6, 22e-9),                /* C10                                  */
    R_(6, 7, 2.2e3),                /* R14                                  */
    R_(7, 2, 6.8e3),                /* R15                                  */
    POT_(5, 8, 7, 25e3, K_TONE, LIN),   /* TONE 25 kB                       */
    R_(8, 2, 100e3),                /* LEVEL (full, as load)                */
    END_
};
static const bf_netlist DS_BOOST_NL = NL(DS_BOOST, 10, 7, 0);
static const bf_netlist DS_GAIN_NL  = NL(DS_GAIN, 12, 6, 0);
static const bf_netlist DS_CLIP_NL  = NL(DS_CLIP, 9, 8, 2);

static void ds1_map(const bf_ui *ui, float *k, float *g, bf_elem *w[]) {
    k[K_DRIVE] = ui->drive; k[K_TONE] = ui->tone; *g = 1.0f;
    bf_elem *d = find(w[2], BF_E_DPAIR, 0);
    if (ui->voice == 1) set_pair(d, BF_D_LED_RED, 1, 1);
    else if (ui->voice == 2) set_pair(d, BF_D_1N914, 2, 2);
    else set_pair(d, BF_D_1N914, 1, 1);
}

/* ===========================================================================
 * 6. Electro-Harmonix Big Muff Pi
 * ======================================================================== */
static const bf_elem BMP_IN[] = {
    V_(1, 0, 0, 1.0),
    V_(2, 0, 1, 9.0),
    R_(1, 3, 33e3),                 /* R2                                   */
    C_(3, 4, 100e-9),               /* C1                                   */
    R_(4, 0, 100e3),                /* R3                                   */
    R_(4, 5, 470e3),                /* R4                                   */
    C_(4, 5, 470e-12),              /* C2                                   */
    R_(5, 2, 15e3),                 /* R6                                   */
    Q_(5, 4, 6, BF_Q_2N3904),       /* Q1                                   */
    R_(6, 0, 100),                  /* R5                                   */
    END_
};
static const bf_elem BMP_CLIP1[] = {
    V_(1, 0, 0, 1.0),
    V_(2, 0, 1, 9.0),
    R_(1, 3, 15e3),                 /* Q1 collector source (R6)             */
    C_(3, 4, 100e-9),               /* C3                                   */
    POT_(6, 5, 4, 100e3, K_DRIVE, LIN),   /* SUSTAIN 100 kB                 */
    R_(6, 0, 820),                  /* R7                                   */
    C_(5, 7, 100e-9),               /* C4                                   */
    R_(7, 8, 8.2e3),                /* R8                                   */
    R_(8, 0, 100e3),                /* R9                                   */
    R_(8, 9, 470e3),                /* R10                                  */
    C_(8, 9, 470e-12),              /* C5                                   */
    C_(8, 11, 100e-9),              /* C6                                   */
    DP_(11, 9, BF_D_1N914, 1, BF_D_1N914, 1),  /* D2/D3                     */
    R_(9, 2, 10e3),                 /* R12                                  */
    QFA_(9, 8, 10, BF_Q_2N3904),    /* Q2                                   */
    R_(10, 0, 100),                 /* R11                                  */
    END_
};
static const bf_elem BMP_CLIP2[] = {
    V_(1, 0, 0, 1.0),
    V_(2, 0, 1, 9.0),
    R_(1, 3, 10e3),                 /* Q2 collector source (R12)            */
    C_(3, 4, 100e-9),               /* C7                                   */
    R_(4, 5, 8.2e3),                /* R13                                  */
    R_(5, 0, 100e3),                /* R14                                  */
    R_(5, 6, 470e3),                /* R15                                  */
    C_(5, 6, 470e-12),              /* C8                                   */
    C_(5, 8, 100e-9),               /* C9                                   */
    DP_(8, 6, BF_D_1N914, 1, BF_D_1N914, 1),  /* D4/D5                      */
    R_(6, 2, 15e3),                 /* R17                                  */
    QFA_(6, 5, 7, BF_Q_2N3904),     /* Q3                                   */
    R_(7, 0, 100),                  /* R16                                  */
    R_(6, 9, 33e3),                 /* R19 tone LPF                         */
    C_(9, 0, 10e-9),                /* C11                                  */
    C_(6, 10, 3.9e-9),              /* C10 tone HPF                         */
    R_(10, 0, 33e3),                /* R18                                  */
    POT_(9, 11, 10, 100e3, K_TONE, LIN),  /* TONE 100 kB                    */
    C_(11, 12, 100e-9),             /* C12                                  */
    R_(12, 2, 390e3),               /* R20                                  */
    R_(12, 0, 100e3),               /* R21                                  */
    END_
};
static const bf_elem BMP_OUT[] = {
    V_(1, 0, 0, 1.0),
    V_(2, 0, 1, 9.0),
    QFA_(3, 1, 4, BF_Q_2N3904),     /* Q4 recovery stage                    */
    R_(4, 0, 2.7e3),                /* R23                                  */
    R_(3, 2, 12e3),                 /* R22                                  */
    C_(3, 5, 100e-9),               /* C13                                  */
    R_(5, 0, 100e3),                /* VOLUME (full, as load)               */
    END_
};
static const bf_netlist BMP_IN_NL    = NL(BMP_IN, 7, 5, 0);
static const bf_netlist BMP_CLIP1_NL = NL(BMP_CLIP1, 12, 9, 0);
static const bf_netlist BMP_CLIP2_NL = NL(BMP_CLIP2, 13, 12, 0);
static const bf_netlist BMP_OUT_NL   = NL(BMP_OUT, 6, 5, 0);

static void bmp_map(const bf_ui *ui, float *k, float *g, bf_elem *w[]) {
    k[K_DRIVE] = ui->drive; k[K_TONE] = ui->tone; *g = 1.0f;
    /* capacitor order in BMP_CLIP2: C7, C8, C9, C11, C10, C12 */
    bf_elem *c11 = find(w[2], BF_E_C, 3), *c10 = find(w[2], BF_E_C, 4);
    if (!c11 || !c10) return;
    if (ui->voice == 1) { c10->value = 10e-9f; c11->value = 3.9e-9f; }  /* mids */
    else                { c10->value = 3.9e-9f; c11->value = 10e-9f; }  /* stock */
}

/* ===========================================================================
 * Shared guitar front end for transistor fuzzes: pickup (R, L, C) and the
 * guitar's 250 kA volume pot.  Fuzz Face / Rangemaster input impedance is
 * low enough that this interaction is part of the sound ("cleans up with
 * the guitar volume").
 * ======================================================================== */
#define PICKUP_(in, mid, pk, gv, out) \
    R_(in, mid, 6.5e3),             /* pickup DC resistance                 */ \
    L_(mid, pk, 2.5),               /* pickup inductance                    */ \
    C_(pk, 0, 680e-12),             /* pickup + cable capacitance           */ \
    POT_(0, out, pk, 250e3, K_TONE, LOGA)   /* guitar VOLUME 250 kA          */

static void pickup_voice(bf_elem *w, uint8_t voice) {
    bf_elem *r = find(w, BF_E_R, 0), *l = find(w, BF_E_L, 0);
    if (!r || !l) return;
    switch (voice) {
    case 1:  r->value = 8.5e3f; l->value = 4.5f;  break;   /* humbucker     */
    case 2:  r->value = 10.0f;  l->value = 1e-4f; break;   /* buffered      */
    default: r->value = 6.5e3f; l->value = 2.5f;  break;   /* single coil   */
    }
}

/* ===========================================================================
 * 7/8. Dallas-Arbiter Fuzz Face (germanium PNP / silicon NPN)
 * ======================================================================== */
#define FUZZFACE(VSUP, Q1M, Q2M) { \
    V_(1, 0, 0, 1.0), \
    V_(2, 0, 1, VSUP), \
    PICKUP_(1, 4, 3, 0, 12), \
    C_(12, 5, 2.2e-6),              /* C1 2.2 µF                           */ \
    Q_(6, 5, 0, Q1M),               /* Q1                                   */ \
    R_(6, 2, 33e3),                 /* R2 33 k                              */ \
    Q_(7, 6, 8, Q2M),               /* Q2                                   */ \
    R_(7, 9, 8.2e3),                /* R3 8k2                               */ \
    R_(9, 2, 470),                  /* R4 470                               */ \
    R_(5, 8, 100e3),                /* R1 100 k feedback                    */ \
    POT_(0, 10, 8, 1e3, K_DRIVE, REVC),   /* FUZZ 1 kC                      */ \
    C_(10, 0, 22e-6),               /* C2 22 µF                             */ \
    END_ }

/* Junction capacitance (Cob) is left out here: in the audible band it moved
 * the 2× aliasing floor by < 2 dB while costing two more solver states on
 * the most expensive model (the Rangemaster below keeps it — it's cheap). */
static const bf_elem FF_GE[] = FUZZFACE(-9.0, BF_Q_NKT275_Q1, BF_Q_NKT275_Q2);
static const bf_elem FF_SI[] = FUZZFACE(+9.0, BF_Q_BC108_Q1, BF_Q_BC108_Q2);
static const bf_netlist FF_GE_NL = NL(FF_GE, 13, 9, 0);
static const bf_netlist FF_SI_NL = NL(FF_SI, 13, 9, 0);

/* Output coupling into the volume pot / amp, at base rate.  Its 500 k load
 * on the 8k2 collector node is a 0.14 dB effect and is left out of the
 * oversampled transistor stage (saves one state there). */
static const bf_elem FF_OUT[] = {
    V_(1, 0, 0, 1.0),
    C_(1, 2, 10e-9),                /* C3 10 n                              */
    R_(2, 0, 500e3),                /* VOLUME 500 kA (full, as load)        */
    END_
};
static const bf_netlist FF_OUT_NL = NL(FF_OUT, 3, 2, 0);

static void ff_map(const bf_ui *ui, float *k, float *g, bf_elem *w[]) {
    k[K_DRIVE] = ui->drive;
    k[K_TONE]  = ui->tone;                 /* guitar volume */
    *g = 1.0f;
    pickup_voice(w[0], ui->voice);
}

/* ===========================================================================
 * 9. Dallas Rangemaster treble booster (OC44)
 * ======================================================================== */
static const bf_elem RM[] = {
    V_(1, 0, 0, 1.0),
    V_(2, 0, 1, -9.0),
    PICKUP_(1, 4, 3, 0, 12),
    C_(12, 5, 5e-9),                /* C1 5 n (treble range)                */
    R_(5, 2, 470e3),                /* R1                                   */
    R_(5, 0, 68e3),                 /* R2                                   */
    Q_(7, 5, 6, BF_Q_OC44),         /* Q1 OC44                              */
    CJ_(5, 7, 20e-12),              /* Cob (alloy Ge, estimate)             */
    R_(6, 0, 3.9e3),                /* R3                                   */
    C_(6, 0, 47e-6),                /* C2                                   */
    R_(7, 2, 10e3),                 /* BOOST 10 k (collector load)          */
    C_(7, 8, 10e-9),                /* C3                                   */
    R_(8, 0, 1e6),                  /* amp input                            */
    END_
};
static const bf_netlist RM_NL = NL(RM, 13, 8, 0);

static void rm_map(const bf_ui *ui, float *k, float *g, bf_elem *w[]) {
    k[K_TONE] = ui->tone;                  /* guitar volume */
    /* DRIVE = pickup output level: −12 … +12 dB around a typical guitar */
    *g = powf(10.0f, (ui->drive * 24.0f - 12.0f) / 20.0f);
    pickup_voice(w[0], ui->voice);
}

/* ===========================================================================
 * Model table
 * ======================================================================== */
#define NO_RAIL 0.0f, 0.0f, 0.0f

const bf_model_desc BF_MODELS[BF_MODEL_COUNT] = {
    { "TS808", 2,
      { { &TS_CLIP_NL, 1, 0, NO_RAIL }, { &TS_TONE_NL, 0, 0, NO_RAIL } },
      4, 4, { "Si", "Asym", "Si x2", "LED" }, "Drive", "Tone", 1.3964f /* +2.9 dB */, 0.6f, 0.5f, ts_map },

    { "KLON", 3,
      { { &KLON_GAIN_NL, 0, 0, NO_RAIL }, { &KLON_MAIN_NL, 1, 1, NO_RAIL },
        { &KLON_TONE_NL, 0, 0, NO_RAIL } },
      2, 2, { "Ge", "Si" }, "Gain", "Treble", 0.1549f /* -16.2 dB */, 0.6f, 0.5f, klon_map },

    { "DIST+", 2,
      { { &DP_GAIN_NL, 0, 0, 1.7f, 7.3f, 0.5e6f }, { &DP_CLIP_NL, 1, 0, NO_RAIL } },
      4, 3, { "Ge MXR", "Si DOD", "LED" }, "Dist", "n/a", 1.4962f /* +3.5 dB */, 0.6f, 0.5f, dplus_map },

    { "RAT", 2,
      { { &RAT_GAIN_NL, 0, 0, 1.2f, 7.8f, 0.3e6f }, { &RAT_CLIP_NL, 1, 0, NO_RAIL } },
      4, 3, { "Si", "LED Turbo", "Ge" }, "Dist", "Filter", 0.4732f /* -6.5 dB */, 0.6f, 0.5f, rat_map },

    { "DS-1", 3,
      { { &DS_BOOST_NL, 1, 0, NO_RAIL }, { &DS_GAIN_NL, 1, 0, 0.8f, 7.6f, 1.0e6f },
        { &DS_CLIP_NL, 1, 0, NO_RAIL } },
      2, 3, { "Si", "LED", "Si x2" }, "Dist", "Tone", 1.7579f /* +4.9 dB */, 0.6f, 0.5f, ds1_map },

    { "BIG MUFF", 4,
      { { &BMP_IN_NL, 0, 0, NO_RAIL }, { &BMP_CLIP1_NL, 1, 0, NO_RAIL },
        { &BMP_CLIP2_NL, 1, 0, NO_RAIL }, { &BMP_OUT_NL, 0, 0, NO_RAIL } },
      2, 2, { "Stock", "Mids" }, "Sustain", "Tone", 0.4955f /* -6.1 dB */, 0.6f, 0.5f, bmp_map },

    { "FUZZFACE Ge", 2, { { &FF_GE_NL, 1, 0, NO_RAIL }, { &FF_OUT_NL, 0, 0, NO_RAIL } },
      2, 3, { "Single", "Humbck", "Buffer" }, "Fuzz", "Gtr Vol", 1.1220f /* +1.0 dB */, 0.6f, 1.0f, ff_map },

    { "FUZZFACE Si", 2, { { &FF_SI_NL, 1, 0, NO_RAIL }, { &FF_OUT_NL, 0, 0, NO_RAIL } },
      2, 3, { "Single", "Humbck", "Buffer" }, "Fuzz", "Gtr Vol", 1.2882f /* +2.2 dB */, 0.6f, 1.0f, ff_map },

    { "RANGEMSTR", 1, { { &RM_NL, 1, 0, NO_RAIL } },
      2, 3, { "Single", "Humbck", "Buffer" }, "Input", "Gtr Vol", 0.6683f /* -3.5 dB */, 0.6f, 1.0f, rm_map },
};
