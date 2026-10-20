/* Audio hot path: build at -O2 even when the Arduino sketch uses -Os, and keep
 * GCC from turning small copy loops into memcpy/memset library calls. */
#pragma GCC optimize ("O2", "no-tree-loop-distribute-patterns")

#include "ifx_bf_circuit.h"
#include <string.h>

/* ===========================================================================
 * Device models (SPICE parameter subsets — see BOOST_FACTORY.md for sources)
 * ======================================================================== */
const bf_diode_model BF_DIODES[BF_D_COUNT] = {
    [BF_D_NONE]    = { 0.0f,     1.0f   },
    [BF_D_1N914]   = { 2.52e-9f, 1.752f },   /* 1N914/1N4148 (common model)  */
    [BF_D_1N34A]   = { 2.0e-7f,  1.3f   },   /* Ge point-contact, ~0.28 V@1mA */
    [BF_D_LED_RED] = { 1.2e-19f, 1.9f   },   /* red LED, ~1.8 V @ 1 mA        */
};

const bf_bjt_model BF_BJTS[BF_Q_COUNT] = {
    [BF_Q_2N3904]    = { 6.734e-15f, 416.4f, 0.7371f, 1.0f, 0 },
    [BF_Q_2N5088]    = { 5.911e-15f, 1122.f, 1.271f,  1.0f, 0 },
    [BF_Q_NKT275_Q1] = { 1.0e-6f,    75.0f,  5.0f,    1.0f, 1 },  /* Ge PNP */
    [BF_Q_NKT275_Q2] = { 1.0e-6f,    115.0f, 5.0f,    1.0f, 1 },
    [BF_Q_BC108_Q1]  = { 1.8e-14f,   120.0f, 3.0f,    1.0f, 0 },  /* Si NPN */
    [BF_Q_BC108_Q2]  = { 1.8e-14f,   200.0f, 3.0f,    1.0f, 0 },
    [BF_Q_OC44]      = { 1.0e-6f,    80.0f,  5.0f,    1.0f, 1 },  /* Ge PNP */
};

float bf_taper(uint8_t taper, float k) {
    if (k < 0.0f) k = 0.0f;
    if (k > 1.0f) k = 1.0f;
    /* Audio taper: 10 % of the track at mid rotation (k = 2·ln 9). */
    const float ka = 4.39444915f, den = 80.0f;      /* e^ka − 1 = 80 */
    switch (taper) {
    case BF_TAPER_A: return (expf(ka * k) - 1.0f) / den;
    case BF_TAPER_C: return 1.0f - (expf(ka * (1.0f - k)) - 1.0f) / den;
    default:         return k;
    }
}

/* ===========================================================================
 * Build workspace (UI core only — not reentrant)
 * ======================================================================== */
#define BF_MAX_UNK   (BF_MAX_NODES + 24)
#define BF_MAX_COLS  (BF_MAX_SRC + BF_MAX_STATES + BF_MAX_PORTS)

static double s_G[BF_MAX_UNK][BF_MAX_UNK];
static double s_X[BF_MAX_UNK][BF_MAX_COLS];

typedef struct {
    int    nUnk, N, P;
    int    stA[BF_MAX_STATES], stB[BF_MAX_STATES];   /* unknown idx or −1 */
    double stG[BF_MAX_STATES];
    int8_t stInd[BF_MAX_STATES];
    int    pA[BF_MAX_PORTS], pB[BF_MAX_PORTS];        /* port sense nodes   */
    /* cap-voltage rows (for DC init) */
    double Vx[BF_MAX_STATES][BF_MAX_STATES];
    double Vu[BF_MAX_STATES][BF_MAX_SRC];
    double Vi[BF_MAX_STATES][BF_MAX_PORTS];
} bf_build_info;

static bf_build_info s_info;
static uint8_t pick_kernel(int N, int P, bool aux);

static inline int ui(int node) { return node - 1; }   /* node → unknown, −1 = gnd */

static void stamp_g(int a, int b, double g) {
    int ia = ui(a), ib = ui(b);
    if (ia >= 0) s_G[ia][ia] += g;
    if (ib >= 0) s_G[ib][ib] += g;
    if (ia >= 0 && ib >= 0) { s_G[ia][ib] -= g; s_G[ib][ia] -= g; }
}

static void inj(int col, int node, double v) {
    int i = ui(node);
    if (i >= 0) s_X[i][col] += v;
}

static void fill_port(bf_port *pt, const bf_diode_model *df, float nf,
                      const bf_diode_model *dr, float nr) {
    memset(pt, 0, sizeof(*pt));
    pt->type = BF_PORT_DPAIR;
    pt->Isf = df ? df->Is : 0.0f;
    pt->af  = df ? df->N * BF_VT * nf : 1.0f;
    pt->Isr = dr ? dr->Is : 0.0f;
    pt->ar  = dr ? dr->N * BF_VT * nr : 1.0f;
}

/* LU-free Gaussian elimination with partial pivoting on [G | X]. */
static bool solve_system(int n, int ncol) {
    for (int c = 0; c < n; c++) {
        int piv = c;
        double best = fabs(s_G[c][c]);
        for (int r = c + 1; r < n; r++) {
            double a = fabs(s_G[r][c]);
            if (a > best) { best = a; piv = r; }
        }
        if (best < 1e-30) return false;
        if (piv != c) {
            for (int k = 0; k < n; k++)    { double t = s_G[c][k]; s_G[c][k] = s_G[piv][k]; s_G[piv][k] = t; }
            for (int k = 0; k < ncol; k++) { double t = s_X[c][k]; s_X[c][k] = s_X[piv][k]; s_X[piv][k] = t; }
        }
        double inv = 1.0 / s_G[c][c];
        for (int r = c + 1; r < n; r++) {
            double f = s_G[r][c] * inv;
            if (f == 0.0) continue;
            for (int k = c; k < n; k++)    s_G[r][k] -= f * s_G[c][k];
            for (int k = 0; k < ncol; k++) s_X[r][k] -= f * s_X[c][k];
        }
    }
    for (int r = n - 1; r >= 0; r--) {
        double inv = 1.0 / s_G[r][r];
        for (int k = 0; k < ncol; k++) {
            double acc = s_X[r][k];
            for (int c = r + 1; c < n; c++) acc -= s_G[r][c] * s_X[c][k];
            s_X[r][k] = acc * inv;
        }
    }
    return true;
}

static double row_diff(int a, int b, int col) {
    double va = (a >= 0) ? s_X[a][col] : 0.0;
    double vb = (b >= 0) ? s_X[b][col] : 0.0;
    return va - vb;
}

static bool build_core(bf_dk *dk, const bf_netlist *nl, double T, const float *knobs) {
    bf_build_info *in = &s_info;
    memset(in, 0, sizeof(*in));
    memset(s_G, 0, sizeof(s_G));
    memset(s_X, 0, sizeof(s_X));
    memset(dk, 0, sizeof(*dk));

    const int nNodeUnk = nl->nNodes - 1;
    int nUnk = nNodeUnk;
    const int M = BF_MAX_SRC;
    int N = 0, P = 0;

    /* ---- pass 1: allocate extra unknowns, states and ports ---------------*/
    int extra[BF_MAX_ELEMS], inode[BF_MAX_ELEMS];
    for (int e = 0; e < nl->nElems; e++) {
        const bf_elem *el = &nl->elems[e];
        extra[e] = inode[e] = -1;
        switch (el->kind) {
        case BF_E_V: case BF_E_BUF: extra[e] = nUnk++; break;
        case BF_E_OPAMP:
            if (el->value > 0.0f) inode[e] = nUnk++;   /* internal gain node */
            extra[e] = nUnk++;
            break;
        default: break;
        }
    }
    if (nUnk > BF_MAX_UNK) return false;

    /* gmin keeps cap-only nodes solvable at DC */
    for (int i = 0; i < nUnk; i++) s_G[i][i] += 0.0;
    for (int i = 0; i < nNodeUnk; i++) s_G[i][i] += 1e-12;

    for (int e = 0; e < nl->nElems; e++) {
        const bf_elem *el = &nl->elems[e];
        switch (el->kind) {
        case BF_E_R:
            stamp_g(el->n1, el->n2, 1.0 / (double)el->value);
            break;

        case BF_E_POT: {
            double t  = bf_taper(el->aux2, knobs ? knobs[el->aux] : 0.5f);
            double R  = el->value;
            double r12 = R * t, r23 = R * (1.0 - t);
            if (r12 < 0.5) r12 = 0.5;
            if (r23 < 0.5) r23 = 0.5;
            if (el->n1 != el->n2) stamp_g(el->n1, el->n2, 1.0 / r12);
            if (el->n2 != el->n3) stamp_g(el->n2, el->n3, 1.0 / r23);
        } break;

        case BF_E_C: case BF_E_L: {
            if (N >= BF_MAX_STATES) return false;
            bool isL = (el->kind == BF_E_L);
            double g = isL ? T / (2.0 * el->value) : 2.0 * el->value / T;
            stamp_g(el->n1, el->n2, g);
            in->stA[N] = ui(el->n1); in->stB[N] = ui(el->n2);
            in->stG[N] = g; in->stInd[N] = isL;
            N++;
        } break;

        case BF_E_V: {
            int j = extra[e], a = ui(el->n1), b = ui(el->n2);
            if (a >= 0) { s_G[a][j] += 1.0; s_G[j][a] += 1.0; }
            if (b >= 0) { s_G[b][j] -= 1.0; s_G[j][b] -= 1.0; }
            s_X[j][el->aux] += el->value;            /* source column */
        } break;

        case BF_E_BUF: {
            int j = extra[e], o = ui(el->n3), i = ui(el->n1);
            if (o >= 0) { s_G[o][j] += 1.0; s_G[j][o] += 1.0; }
            if (i >= 0) s_G[j][i] -= 1.0;
        } break;

        case BF_E_OPAMP: {
            int j = extra[e], o = ui(el->n3), p = ui(el->n1), m = ui(el->n2);
            if (o >= 0) s_G[o][j] += 1.0;
            if (el->value <= 0.0f) {                 /* ideal nullor */
                if (p >= 0) s_G[j][p] += 1.0;
                if (m >= 0) s_G[j][m] -= 1.0;
            } else {                                 /* single-pole macro */
                const double A0 = 1e5;
                int x = inode[e];
                /* VCCS gm=1: current (v+ − v−) injected into x */
                if (p >= 0) s_G[x][p] -= 1.0;
                if (m >= 0) s_G[x][m] += 1.0;
                s_G[x][x] += 1.0 / A0;
                /* C0 = 1/(2π·GBW) → GBW = gm/(2π·C0) */
                if (N >= BF_MAX_STATES) return false;
                double g = 2.0 * (1.0 / (2.0 * 3.14159265358979 * el->value)) / T;
                s_G[x][x] += g;
                in->stA[N] = x; in->stB[N] = -1; in->stG[N] = g; in->stInd[N] = 0;
                N++;
                /* VCVS: v(out) = v(x) */
                if (o >= 0) s_G[j][o] += 1.0;
                s_G[j][x] -= 1.0;
            }
        } break;

        case BF_E_DPAIR: {
            if (P >= BF_MAX_PORTS) return false;
            fill_port(&dk->port[P],
                      &BF_DIODES[el->aux], (float)el->aux2,
                      el->n3 ? &BF_DIODES[el->n3] : 0, el->value);
            in->pA[P] = ui(el->n1); in->pB[P] = ui(el->n2);
            int col = M + BF_MAX_STATES + P;
            inj(col, el->n1, -1.0); inj(col, el->n2, +1.0);
            P++;
        } break;

        case BF_E_BJTFA: {
            if (P >= BF_MAX_PORTS) return false;
            const bf_bjt_model *q = &BF_BJTS[el->aux];
            bf_port *pt = &dk->port[P];
            memset(pt, 0, sizeof(*pt));
            pt->type = BF_PORT_FA;
            float Ise = q->Is * (1.0f + 1.0f / q->Bf);
            float a = q->Nf * BF_VT;
            if (q->pnp) { pt->Isr = Ise; pt->ar = a; pt->af = a; }
            else        { pt->Isf = Ise; pt->af = a; pt->ar = a; }
            in->pA[P] = ui(el->n2); in->pB[P] = ui(el->n3);    /* vbe */
            int col = M + BF_MAX_STATES + P;
            double bf = q->Bf;
            inj(col, el->n2, -1.0 / (bf + 1.0));
            inj(col, el->n1, -bf / (bf + 1.0));
            inj(col, el->n3, +1.0);
            P++;
        } break;

        case BF_E_BJT: {
            if (P + 2 > BF_MAX_PORTS) return false;
            const bf_bjt_model *q = &BF_BJTS[el->aux];
            for (int k = 0; k < 2; k++) {
                bf_port *pt = &dk->port[P + k];
                memset(pt, 0, sizeof(*pt));
                pt->type  = k ? BF_PORT_BJT_BC : BF_PORT_BJT_BE;
                pt->Is  = q->Is;
                pt->kF  = 1.0f + 1.0f / q->Bf;
                pt->kR  = 1.0f + 1.0f / q->Br;
                pt->iaF = 1.0f / (q->Nf * BF_VT);
                pt->iaR = 1.0f / (q->Nf * BF_VT);
                pt->sgn = q->pnp ? -1.0f : 1.0f;
            }
            /* port P: vbe, current B→E ; port P+1: vbc, current B→C */
            in->pA[P] = ui(el->n2);     in->pB[P] = ui(el->n3);
            in->pA[P + 1] = ui(el->n2); in->pB[P + 1] = ui(el->n1);
            int c0 = M + BF_MAX_STATES + P, c1 = c0 + 1;
            inj(c0, el->n2, -1.0); inj(c0, el->n3, +1.0);
            inj(c1, el->n2, -1.0); inj(c1, el->n1, +1.0);
            P += 2;
        } break;
        }
    }

    /* state history injection columns */
    for (int k = 0; k < N; k++) {
        int col = M + k;
        double sgn = in->stInd[k] ? -1.0 : 1.0;   /* cap: +s into A; ind: −h */
        if (in->stA[k] >= 0) s_X[in->stA[k]][col] += sgn;
        if (in->stB[k] >= 0) s_X[in->stB[k]][col] -= sgn;
    }

    if (!solve_system(nUnk, BF_MAX_COLS)) return false;

    in->nUnk = nUnk; in->N = N; in->P = P;
    dk->N = (uint8_t)N; dk->P = (uint8_t)P; dk->M = M;

    const int cS = M, cP = M + BF_MAX_STATES;
    /* packed layouts (see header):  Pm row = [u0, 1, x…, u2]
     *                                Sm row = [u0, 1, x…, i…, u2]          */
    const int zX = 2, zI = 2 + N, pU2 = 2 + N, sU2 = 2 + N + P;
    double Kd[BF_MAX_PORTS][BF_MAX_PORTS];
    bool usesAux = false;
    for (int e = 0; e < nl->nElems; e++)
        if (nl->elems[e].kind == BF_E_V && nl->elems[e].aux == 2) usesAux = true;
    dk->aux = usesAux;
    /* port drive rows */
    for (int j = 0; j < P; j++) {
        dk->Pm[j][0]   = (float)row_diff(in->pA[j], in->pB[j], 0);
        dk->Pm[j][1]   = (float)row_diff(in->pA[j], in->pB[j], 1);
        dk->Pm[j][pU2] = (float)row_diff(in->pA[j], in->pB[j], 2);
        for (int k = 0; k < N; k++) dk->Pm[j][zX + k] = (float)row_diff(in->pA[j], in->pB[j], cS + k);
        for (int l = 0; l < P; l++) {
            Kd[j][l] = -row_diff(in->pA[j], in->pB[j], cP + l);
            dk->K[j][l] = (float)Kd[j][l];
        }
    }
    /* state rows: x' = 2G·v_cap ∓ x */
    for (int k = 0; k < N; k++) {
        double g2 = 2.0 * in->stG[k];
        float *row = dk->Sm[k];
        for (int m = 0; m < M; m++) in->Vu[k][m] = row_diff(in->stA[k], in->stB[k], m);
        row[0]   = (float)(g2 * in->Vu[k][0]);
        row[1]   = (float)(g2 * in->Vu[k][1]);
        row[sU2] = (float)(g2 * in->Vu[k][2]);
        for (int l = 0; l < N; l++) {
            in->Vx[k][l] = row_diff(in->stA[k], in->stB[k], cS + l);
            row[zX + l] = (float)(g2 * in->Vx[k][l]);
        }
        row[zX + k] += in->stInd[k] ? 1.0f : -1.0f;
        for (int l = 0; l < P; l++) {
            in->Vi[k][l] = row_diff(in->stA[k], in->stB[k], cP + l);
            row[zI + l] = (float)(g2 * in->Vi[k][l]);
        }
    }
    /* output row */
    {
        int a = ui(nl->outP), b = ui(nl->outN);
        float *row = dk->Sm[N];
        row[0]   = (float)row_diff(a, b, 0);
        row[1]   = (float)row_diff(a, b, 1);
        row[sU2] = (float)row_diff(a, b, 2);
        for (int k = 0; k < N; k++) row[zX + k] = (float)row_diff(a, b, cS + k);
        for (int l = 0; l < P; l++) row[zI + l] = (float)row_diff(a, b, cP + l);
    }
    /* solver choice + per-port reciprocals */
    dk->solver = P == 0 ? BF_SOLVE_NONE
               : (P == 1 && dk->port[0].type != BF_PORT_BJT_BE) ? BF_SOLVE_CLOSED
               : BF_SOLVE_NEWTON;
    dk->kernel = pick_kernel(N, P, usesAux);
    for (int j = 0; j < P; j++) {
        bf_port *pt = &dk->port[j];
        if (pt->type == BF_PORT_DPAIR || pt->type == BF_PORT_FA) {
            pt->iaf = 1.0f / pt->af;
            pt->iar = 1.0f / pt->ar;
        }
    }
    if (dk->solver == BF_SOLVE_CLOSED) {
        double K = Kd[0][0];
        if (K < 1e-6) K = 1e-6;
        const bf_port *pt = &dk->port[0];
        dk->Kf = (float)K;
        dk->invK = (float)(1.0 / K);
        dk->lnKIs_f = pt->Isf > 0.0f ? (float)log(K * pt->Isf / pt->af) : 0.0f;
        dk->lnKIs_r = pt->Isr > 0.0f ? (float)log(K * pt->Isr / pt->ar) : 0.0f;
    }
    return true;
}

bool bf_dk_build(bf_dk *dk, const bf_netlist *nl, double T, const float *knobs) {
    return build_core(dk, nl, T, knobs);
}

/* ===========================================================================
 * Nonlinear port solvers (audio core)
 * ======================================================================== */

/* v + K·Is·(e^{v/a} − 1) = p  for p ≥ 0, closed form through Wright omega.
 * Uses the log form when the junction conducts (avoids cancellation). */
static inline float wo_branch(float p, float K, float Is, float ia, float a, float lnKIs) {
    float q = p + K * Is;
    float w = bf_omega(lnKIs + q * ia);
    if (w > 1.0f) return a * (bf_log(w) - lnKIs);
    return q - a * w;
}

static inline float closed_form(const bf_dk *dk, float p) {
    const bf_port *pt = &dk->port[0];
    if (p >= 0.0f) {
        if (pt->Isf <= 0.0f) return p;
        return wo_branch(p, dk->Kf, pt->Isf, pt->iaf, pt->af, dk->lnKIs_f);
    }
    if (pt->Isr <= 0.0f) return p;
    return -wo_branch(-p, dk->Kf, pt->Isr, pt->iar, pt->ar, dk->lnKIs_r);
}

/* Device currents i(v) and Jacobian di/dv (row-major P×P; only the device
 * blocks are written — caller zeroes J). */
static inline void dev_eval(const bf_dk *dk, const float *v, float *i, float *J, int P) {
    for (int j = 0; j < P; ) {
        const bf_port *pt = &dk->port[j];
        if (pt->type == BF_PORT_BJT_BE) {
            /* Ebers-Moll transport model; PNP handled by i(v) = −i_npn(−v) */
            const float s = pt->sgn;
            float eF = bf_exp(s * v[j] * pt->iaF);
            float eR = bf_exp(s * v[j + 1] * pt->iaR);
            float If = pt->Is * (eF - 1.0f), Ir = pt->Is * (eR - 1.0f);
            float dF = pt->Is * eF * pt->iaF, dR = pt->Is * eR * pt->iaR;
            i[j]     = s * (If * pt->kF - Ir);        /*  Ie (B→E) */
            i[j + 1] = s * (Ir * pt->kR - If);        /* −Ic (B→C) */
            J[j * P + j]           =  dF * pt->kF;
            J[j * P + j + 1]       = -dR;
            J[(j + 1) * P + j]     = -dF;
            J[(j + 1) * P + j + 1] =  dR * pt->kR;
            j += 2;
        } else {
            float ef = pt->Isf > 0.0f ? bf_exp(v[j] * pt->iaf) : 1.0f;
            float er = pt->Isr > 0.0f ? bf_exp(-v[j] * pt->iar) : 1.0f;
            i[j] = pt->Isf * (ef - 1.0f) - pt->Isr * (er - 1.0f);
            J[j * P + j] = pt->Isf * ef * pt->iaf + pt->Isr * er * pt->iar;
            j += 1;
        }
    }
}

/* Damped Newton on F(v) = v + K·i(v) − p for P ≤ 4 ports.
 * Converged when the largest step < 100 µV (−80 dB re 1 V; float noise floor
 * of the high-impedance fuzz networks sits around 10–50 µV); currents are then updated with
 * the Newton linearisation (error O(step²)), saving a final exp evaluation. */
static int IFX_RAMFUNC(newton)(const bf_dk *dk, const float *p, float *v, float *i, int maxIt) {
    const int P = dk->P;
    float J[BF_MAX_PORTS * BF_MAX_PORTS];
    float M[BF_MAX_PORTS][BF_MAX_PORTS + 1];
    int it = 0;
    for (;;) {
        for (int k = 0; k < P * P; k++) J[k] = 0.0f;
        dev_eval(dk, v, i, J, P);
        for (int j = 0; j < P; j++) {
            const float *Kr = dk->K[j];
            float f = v[j] - p[j];
            for (int l = 0; l < P; l++) f += Kr[l] * i[l];
            M[j][P] = -f;
            for (int l = 0; l < P; l++) {
                /* (K·J)[j][l]: J column l is nonzero only inside l's device block */
                int q0 = l, q1 = l;
                uint8_t t = dk->port[l].type;
                if (t == BF_PORT_BJT_BE) q1 = l + 1;
                else if (t == BF_PORT_BJT_BC) q0 = l - 1;
                float m = (j == l) ? 1.0f : 0.0f;
                for (int q = q0; q <= q1; q++) m += Kr[q] * J[q * P + l];
                M[j][l] = m;
            }
        }
        /* Gaussian elimination with partial pivoting (P ≤ 4) */
        for (int c = 0; c < P; c++) {
            int piv = c;
            for (int r = c + 1; r < P; r++) if (fabsf(M[r][c]) > fabsf(M[piv][c])) piv = r;
            if (piv != c) for (int k = c; k <= P; k++) { float t = M[c][k]; M[c][k] = M[piv][k]; M[piv][k] = t; }
            float inv = 1.0f / M[c][c];
            for (int r = c + 1; r < P; r++) {
                float f = M[r][c] * inv;
                for (int k = c + 1; k <= P; k++) M[r][k] -= f * M[c][k];
            }
            M[c][c] = inv;
        }
        float d[BF_MAX_PORTS], maxd = 0.0f;
        for (int r = P - 1; r >= 0; r--) {
            float acc = M[r][P];
            for (int c = r + 1; c < P; c++) acc -= M[r][c] * d[c];
            float s = acc * M[r][r];
            if (s >  0.1f) s =  0.1f;           /* junction step limiting */
            if (s < -0.1f) s = -0.1f;
            d[r] = s;
            /* a non-conducting junction (tiny di/dv) can't move any current:
             * its voltage jitter at the float noise floor is irrelevant */
            float a = J[r * P + r] > 1e-6f ? fabsf(s) : 0.0f;
            if (a > maxd) maxd = a;
        }
        for (int j = 0; j < P; j++) v[j] += d[j];
        it++;
        if (maxd < 1e-4f || it >= maxIt) {
            for (int j = 0; j < P; j++) {
                float acc = i[j];
                for (int l = 0; l < P; l++) acc += J[j * P + l] * d[l];
                i[j] = acc;
            }
            return it;
        }
    }
}

/* ---- Specialised Newton for the common 2- and 4-port cases --------------
 * Same maths as newton() above, fully unrolled.  Diode pairs only evaluate
 * the branch matching the sign of v: the other branch carries ≤ Is (nA). */

static inline void port1_eval(const bf_port *pt, float v, float *i, float *g) {
    if (v >= 0.0f) {
        float e = bf_exp(v * pt->iaf);
        *i = pt->Isf * (e - 1.0f);  *g = pt->Isf * e * pt->iaf;
    } else {
        float e = bf_exp(-v * pt->iar);
        *i = -pt->Isr * (e - 1.0f); *g = pt->Isr * e * pt->iar;
    }
}

static inline void bjt_eval(const bf_port *pt, float vbe, float vbc,
                            float *i0, float *i1, float *J00, float *J01, float *J10, float *J11) {
    const float s = pt->sgn;
    float xF = s * vbe * pt->iaF, xR = s * vbc * pt->iaR;
    float eF = xF > -30.0f ? bf_exp(xF) : 0.0f;     /* reverse-biased: e^x ≈ 0 */
    float eR = xR > -30.0f ? bf_exp(xR) : 0.0f;
    float If = pt->Is * (eF - 1.0f), Ir = pt->Is * (eR - 1.0f);
    float dF = pt->Is * eF * pt->iaF, dR = pt->Is * eR * pt->iaR;
    *i0 = s * (If * pt->kF - Ir);
    *i1 = s * (Ir * pt->kR - If);
    *J00 = dF * pt->kF; *J01 = -dR; *J10 = -dF; *J11 = dR * pt->kR;
}

static inline float limstep(float d) {
    return d > 0.1f ? 0.1f : (d < -0.1f ? -0.1f : d);
}

static inline void newton2(const bf_dk *dk, const float *p, float *v, float *i) {
    const float K00 = dk->K[0][0], K01 = dk->K[0][1], K10 = dk->K[1][0], K11 = dk->K[1][1];
    const bool bjt = dk->port[0].type == BF_PORT_BJT_BE;
    float v0 = v[0], v1 = v[1], i0 = 0, i1 = 0, J00 = 0, J01 = 0, J10 = 0, J11 = 0, d0 = 0, d1 = 0;
    for (int it = 0; it < 6; it++) {
        if (bjt) bjt_eval(&dk->port[0], v0, v1, &i0, &i1, &J00, &J01, &J10, &J11);
        else { port1_eval(&dk->port[0], v0, &i0, &J00); port1_eval(&dk->port[1], v1, &i1, &J11); }
        float F0 = v0 - p[0] + K00 * i0 + K01 * i1;
        float F1 = v1 - p[1] + K10 * i0 + K11 * i1;
        float M00 = 1.0f + K00 * J00 + K01 * J10, M01 = K00 * J01 + K01 * J11;
        float M10 = K10 * J00 + K11 * J10,        M11 = 1.0f + K10 * J01 + K11 * J11;
        float inv = 1.0f / (M00 * M11 - M01 * M10);
        d0 = limstep((M01 * F1 - M11 * F0) * inv);
        d1 = limstep((M10 * F0 - M00 * F1) * inv);
        v0 += d0; v1 += d1;
        float a0 = J00 > 1e-6f ? fabsf(d0) : 0.0f, a1 = J11 > 1e-6f ? fabsf(d1) : 0.0f;
        if (a0 < 1e-4f && a1 < 1e-4f) break;
    }
    i[0] = i0 + J00 * d0 + J01 * d1;
    i[1] = i1 + J10 * d0 + J11 * d1;
    v[0] = v0; v[1] = v1;
}

/* Two Ebers-Moll BJTs (ports 0-1 and 2-3): Fuzz Face.  The Jacobian is
 * block-diagonal (2×2 per transistor), so M = I + K·J needs 2 MACs/entry. */
static inline void newton4(const bf_dk *dk, const float *p, float *v, float *i) {
    float a00 = 0, a01 = 0, a10 = 0, a11 = 0;     /* Q1 block */
    float b00 = 0, b01 = 0, b10 = 0, b11 = 0;     /* Q2 block */
    float ii[4] = { 0, 0, 0, 0 }, d[4] = { 0, 0, 0, 0 };
    for (int it = 0; it < 6; it++) {
        bjt_eval(&dk->port[0], v[0], v[1], &ii[0], &ii[1], &a00, &a01, &a10, &a11);
        bjt_eval(&dk->port[2], v[2], v[3], &ii[2], &ii[3], &b00, &b01, &b10, &b11);
        float M[4][5];
        _Pragma("GCC unroll 4")
        for (int j = 0; j < 4; j++) {
            const float *K = dk->K[j];
            M[j][4] = p[j] - v[j] - (K[0] * ii[0] + K[1] * ii[1] + K[2] * ii[2] + K[3] * ii[3]);
            M[j][0] = K[0] * a00 + K[1] * a10;
            M[j][1] = K[0] * a01 + K[1] * a11;
            M[j][2] = K[2] * b00 + K[3] * b10;
            M[j][3] = K[2] * b01 + K[3] * b11;
        }
        M[0][0] += 1.0f; M[1][1] += 1.0f; M[2][2] += 1.0f; M[3][3] += 1.0f;
        _Pragma("GCC unroll 4")
        for (int c = 0; c < 4; c++) {
            int piv = c;
            _Pragma("GCC unroll 4")
            for (int r = c + 1; r < 4; r++) if (fabsf(M[r][c]) > fabsf(M[piv][c])) piv = r;
            if (piv != c) {
                _Pragma("GCC unroll 5")
                for (int k = 0; k < 5; k++) { float t = M[c][k]; M[c][k] = M[piv][k]; M[piv][k] = t; }
            }
            float inv = 1.0f / M[c][c];
            M[c][c] = inv;
            _Pragma("GCC unroll 4")
            for (int r = c + 1; r < 4; r++) {
                float f = M[r][c] * inv;
                _Pragma("GCC unroll 5")
                for (int k = c + 1; k < 5; k++) M[r][k] -= f * M[c][k];
            }
        }
        d[3] = limstep(M[3][4] * M[3][3]);
        d[2] = limstep((M[2][4] - M[2][3] * d[3]) * M[2][2]);
        d[1] = limstep((M[1][4] - M[1][2] * d[2] - M[1][3] * d[3]) * M[1][1]);
        d[0] = limstep((M[0][4] - M[0][1] * d[1] - M[0][2] * d[2] - M[0][3] * d[3]) * M[0][0]);
        v[0] += d[0]; v[1] += d[1]; v[2] += d[2]; v[3] += d[3];
        /* conducting junctions only (see newton()) */
        float m0 = a00 > 1e-6f ? fabsf(d[0]) : 0.0f, m1 = a11 > 1e-6f ? fabsf(d[1]) : 0.0f;
        float m2 = b00 > 1e-6f ? fabsf(d[2]) : 0.0f, m3 = b11 > 1e-6f ? fabsf(d[3]) : 0.0f;
        if (m0 < 1e-4f && m1 < 1e-4f && m2 < 1e-4f && m3 < 1e-4f) break;
    }
    i[0] = ii[0] + a00 * d[0] + a01 * d[1];
    i[1] = ii[1] + a10 * d[0] + a11 * d[1];
    i[2] = ii[2] + b00 * d[2] + b01 * d[3];
    i[3] = ii[3] + b10 * d[2] + b11 * d[3];
}

/* Port solve for one sample.  z = [u0, 1, x…, i…(, u2)]: x occupies
 * z[2 .. nx), the currents go to z[nx .. nx+P), u2 (if any) is `u2`. */
static inline void solve_ports(const bf_dk *dk, bf_dk_state *st, float *z, int nx, int P, float u2) {
    if (P == 0) return;
    if (dk->solver == BF_SOLVE_CLOSED) {
        float p = dk->Pm[0][nx] * u2;
        _Pragma("GCC unroll 16")
        for (int c = 0; c < nx; c++) p += dk->Pm[0][c] * z[c];
        float v = closed_form(dk, p);
        st->v[0] = v;
        z[nx] = (p - v) * dk->invK;
        return;
    }
    float p[BF_MAX_PORTS], v[BF_MAX_PORTS], i[BF_MAX_PORTS];
    for (int j = 0; j < P; j++) {
        float acc = dk->Pm[j][nx] * u2;
        _Pragma("GCC unroll 16")
        for (int c = 0; c < nx; c++) acc += dk->Pm[j][c] * z[c];
        p[j] = acc;
        float a = st->v[j];
        float dv = a - st->vp[j];                 /* linear predictor */
        if (dv >  0.05f) dv =  0.05f;
        if (dv < -0.05f) dv = -0.05f;
        v[j] = a + dv;
        st->vp[j] = a;
    }
    if (P == 2)      newton2(dk, p, v, i);
    else if (P == 4 && dk->port[0].type == BF_PORT_BJT_BE && dk->port[2].type == BF_PORT_BJT_BE)
                     newton4(dk, p, v, i);
    else             newton(dk, p, v, i, 6);
    for (int j = 0; j < P; j++) {
        if (v[j] != v[j]) { v[j] = 0.0f; i[j] = 0.0f; }   /* NaN guard */
        st->v[j] = v[j];
        z[nx + j] = i[j];
    }
}

/* ---- Size-specialised stage kernels ---------------------------------------
 * Constant N/P lets GCC fully unroll the matrix products; the state vector
 * stays in FPU registers for the whole block (≈2 instructions per MAC). */
#define BF_DK_KERNEL(NN, PP, AX)                                                 \
static void IFX_RAMFUNC(dk_blk_##NN##_##PP##_##AX)(const bf_dk *dk, bf_dk_state *st, \
                                            float *io, const float *aux, int n) { \
    enum { N = NN, P = PP, NX = 2 + NN, NZ = 2 + NN + PP + AX };                 \
    float z[NZ];                                                                 \
    z[1] = 1.0f;                                                                 \
    _Pragma("GCC unroll 16")                                                     \
    for (int k = 0; k < N; k++) z[2 + k] = st->z[2 + k];                         \
    for (int s = 0; s < n; s++) {                                                \
        z[0] = io[s];                                                            \
        float u2 = 0.0f;                                                         \
        if (AX) { u2 = aux[s]; z[NZ - 1] = u2; }                                 \
        _Pragma("GCC unroll 4")                                                  \
        for (int j = NX; j < NX + P; j++) z[j] = 0.0f;                           \
        solve_ports(dk, st, z, NX, P, u2);                                       \
        float y0 = 0.0f, y1 = 0.0f;                                              \
        _Pragma("GCC unroll 24")                                                 \
        for (int c = 0; c + 1 < NZ; c += 2) { y0 += dk->Sm[N][c] * z[c]; y1 += dk->Sm[N][c + 1] * z[c + 1]; } \
        if (NZ & 1) y0 += dk->Sm[N][NZ - 1] * z[NZ - 1];                         \
        float xn[N > 0 ? N : 1];                                                 \
        _Pragma("GCC unroll 16")                                                 \
        for (int k = 0; k < N; k++) {                                            \
            const float *r = dk->Sm[k];                                          \
            float a0 = 0.0f, a1 = 0.0f;                                          \
            _Pragma("GCC unroll 24")                                             \
            for (int c = 0; c + 1 < NZ; c += 2) { a0 += r[c] * z[c]; a1 += r[c + 1] * z[c + 1]; } \
            if (NZ & 1) a0 += r[NZ - 1] * z[NZ - 1];                             \
            xn[k] = a0 + a1;                                                     \
        }                                                                        \
        _Pragma("GCC unroll 16")                                                 \
        for (int k = 0; k < N; k++) z[2 + k] = xn[k];                            \
        io[s] = y0 + y1;                                                         \
    }                                                                            \
    _Pragma("GCC unroll 16")                                                     \
    for (int k = 0; k < N; k++) st->z[2 + k] = z[2 + k];                         \
}

/* Every (N, P, aux) combination used by the shipped models. */
#define BF_KERNEL_LIST(X) \
    X(1,0,0) X(1,1,0) X(2,0,0) X(2,1,0) X(2,2,0) X(3,0,0) X(3,1,0) X(3,2,0) \
    X(4,0,0) X(4,1,0) X(4,2,0) X(4,4,0) X(5,0,0) X(5,2,0) X(5,4,0) X(6,0,0) \
    X(6,2,0) X(8,1,1)

BF_KERNEL_LIST(BF_DK_KERNEL)

typedef void (*bf_kernel_fn)(const bf_dk *, bf_dk_state *, float *, const float *, int);
#define BF_KENTRY(NN, PP, AX) { NN, PP, AX, dk_blk_##NN##_##PP##_##AX },
static const struct { uint8_t n, p, aux; bf_kernel_fn fn; } s_kernels[] = {
    BF_KERNEL_LIST(BF_KENTRY)
};

static uint8_t pick_kernel(int N, int P, bool aux) {
    for (unsigned k = 0; k < sizeof(s_kernels) / sizeof(s_kernels[0]); k++)
        if (s_kernels[k].n == N && s_kernels[k].p == P && s_kernels[k].aux == aux)
            return (uint8_t)(k + 1);
    return 0;
}

/* Generic path for sizes without a specialised kernel. */
static void IFX_RAMFUNC(dk_blk_generic)(const bf_dk *dk, bf_dk_state *st,
                                         float *io, const float *aux, int n) {
    const int N = dk->N, P = dk->P, nx = 2 + N, nz = nx + P + (dk->aux ? 1 : 0);
    float z[BF_ZS], xn[BF_MAX_STATES];
    z[1] = 1.0f;
    for (int k = 0; k < N; k++) z[2 + k] = st->z[2 + k];
    for (int s = 0; s < n; s++) {
        z[0] = io[s];
        float u2 = 0.0f;
        if (dk->aux) { u2 = aux ? aux[s] : 0.0f; z[nz - 1] = u2; }
        for (int j = nx; j < nx + P; j++) z[j] = 0.0f;
        solve_ports(dk, st, z, nx, P, u2);
        float y = 0.0f;
        for (int c = 0; c < nz; c++) y += dk->Sm[N][c] * z[c];
        for (int k = 0; k < N; k++) {
            float acc = 0.0f;
            for (int c = 0; c < nz; c++) acc += dk->Sm[k][c] * z[c];
            xn[k] = acc;
        }
        for (int k = 0; k < N; k++) z[2 + k] = xn[k];
        io[s] = y;
    }
    for (int k = 0; k < N; k++) st->z[2 + k] = z[2 + k];
}

void IFX_RAMFUNC(bf_dk_block)(const bf_dk *dk, bf_dk_state *st, float *io, const float *aux, int n) {
    uint8_t k = dk->kernel;
    if (k) s_kernels[k - 1].fn(dk, st, io, aux, n);
    else   dk_blk_generic(dk, st, io, aux, n);
}

/* ===========================================================================
 * DC operating point
 * ======================================================================== */
static bf_dk s_dcdk;

bool bf_dk_dc_init(bf_dk_state *st, const bf_netlist *nl, double T,
                   const float *knobs, float u0, float *y_dc) {
    /* Caps open / inductors shorted: build with an enormous step. */
    if (!build_core(&s_dcdk, nl, 1e6, knobs)) return false;
    const bf_dk *dk = &s_dcdk;
    const bf_build_info *in = &s_info;
    const int P = dk->P, N = dk->N;

    /* source stepping from 0 → full supply / input */
    float v[BF_MAX_PORTS] = {0}, i[BF_MAX_PORTS] = {0}, p[BF_MAX_PORTS];
    const int steps = 40;
    for (int s = 1; s <= steps; s++) {
        float a = (float)s / steps;
        for (int j = 0; j < P; j++) p[j] = dk->Pm[j][0] * u0 * a + dk->Pm[j][1] * a;
        if (P) newton(dk, p, v, i, 200);
    }

    /* cap voltages at the operating point (states are zero in the DC model) */
    memset(st, 0, sizeof(*st));
    st->z[1] = 1.0f;     /* z[2] (aux input) is 0 at the operating point */
    int k = 0;
    for (int e = 0; e < nl->nElems && k < N; e++) {
        const bf_elem *el = &nl->elems[e];
        double C;
        if (el->kind == BF_E_C) C = el->value;
        else if (el->kind == BF_E_L) { st->z[2 + k++] = 0.0f; continue; }  /* no DC through pickups */
        else if (el->kind == BF_E_OPAMP && el->value > 0.0f) C = 1.0 / (2.0 * 3.14159265358979 * el->value);
        else continue;
        double vk = in->Vu[k][0] * u0 + in->Vu[k][1];   /* aux DC = 0 */
        for (int j = 0; j < P; j++) vk += in->Vi[k][j] * i[j];
        st->z[2 + k] = (float)(2.0 * C / T * vk);      /* zero cap current */
        k++;
    }
    for (int j = 0; j < P; j++) { st->v[j] = v[j]; st->vp[j] = v[j]; st->z[2 + N + j] = i[j]; }

    if (y_dc) {
        const float *row = dk->Sm[N];
        float y = row[0] * u0 + row[1];
        for (int j = 0; j < P; j++) y += row[2 + N + j] * i[j];
        *y_dc = y;
    }
    return true;
}
