#ifndef IFX_BF_CIRCUIT_H
#define IFX_BF_CIRCUIT_H

/*
 * Boost Factory — component-level circuit solver.
 *
 * Each pedal stage is described as a SPICE-like netlist (resistors, caps,
 * inductors, pots, sources, op-amps, diodes, BJTs).  At parameter-change time
 * the netlist is turned into a discrete-time state-space model with the
 * nonlinear devices pulled out as "ports" (the DK / nodal K-method):
 *
 *   p      = D·x + E·u            port drive (Thevenin voltage seen by devices)
 *   v      : v + K·i(v) = p       nonlinear device equation(s), solved per sample
 *   y      = Fx·x + Fu·u + Fi·i   stage output
 *   x[n+1] = A·x + B·u + C·i      capacitor/inductor companion states
 *
 * Reactive parts use the trapezoidal rule (same discretisation SPICE uses by
 * default), so the linear part matches the analog circuit up to bilinear
 * frequency warping — negligible at the 96/192 kHz oversampled rates used.
 *
 * Device ports:
 *   - single diode-pair port      → closed form via the Wright omega function
 *   - forward-active BJT (1 port) → closed form via the Wright omega function
 *   - Ebers-Moll BJT (2 ports) / mixed → damped Newton, warm-started
 *
 * The build runs in double precision on the UI core; the audio core only sees
 * the resulting float matrices.
 */

#include <stdint.h>
#include <stdbool.h>
#include "ifx_bf_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BF_MAX_NODES   28
#define BF_MAX_ELEMS   48
#define BF_MAX_STATES  16
#define BF_MAX_PORTS   4
#define BF_MAX_SRC     3     /* u0 = stage input, u1 = 1.0 (DC), u2 = aux input */
#define BF_MAX_KNOBS   4

#define BF_VT 0.025852f      /* thermal voltage @ 300 K */

typedef enum {
    BF_E_R = 1,   /* n1-n2, value = ohms                                      */
    BF_E_C,       /* n1-n2, value = farads                                    */
    BF_E_L,       /* n1-n2, value = henries                                   */
    BF_E_V,       /* n1(+) n2(-), value = volts per unit of source `aux`:
                     0 = stage input, 1 = DC (1.0), 2 = aux input        */
    BF_E_POT,     /* n1=pin1 n2=wiper n3=pin3, value = total ohms, aux=knob,
                     aux2 = taper; R(1-2) = total·taper(knob)                 */
    BF_E_OPAMP,   /* n1=+ n2=- n3=out, value = GBW Hz (0 = ideal nullor)     */
    BF_E_BUF,     /* n1=in n3=out: ideal unity voltage follower               */
    BF_E_DPAIR,   /* n1=a n2=k: aux = forward diode model, aux2 = count
                     n3 = reverse diode model (0 = none), value = reverse count */
    BF_E_BJT,     /* n1=C n2=B n3=E, aux = BJT model, full Ebers-Moll (2 ports) */
    BF_E_BJTFA,   /* n1=C n2=B n3=E, aux = BJT model, forward-active (1 port) */
} bf_elem_kind;

typedef enum { BF_TAPER_LIN = 0, BF_TAPER_A, BF_TAPER_C } bf_taper_kind;

typedef struct {
    uint8_t kind;
    uint8_t n1, n2, n3;
    uint8_t aux, aux2;
    float   value;
} bf_elem;

typedef struct {
    const bf_elem *elems;
    uint8_t nElems;
    uint8_t nNodes;          /* including ground (node 0) */
    uint8_t outP, outN;      /* output = V(outP) − V(outN) */
} bf_netlist;

/* ---- Device models -------------------------------------------------------*/
typedef struct { float Is, N; } bf_diode_model;
typedef struct { float Is, Bf, Br, Nf; int8_t pnp; } bf_bjt_model;

enum { BF_D_NONE = 0, BF_D_1N914, BF_D_1N34A, BF_D_LED_RED, BF_D_COUNT };
enum { BF_Q_2N3904 = 0, BF_Q_2N5088, BF_Q_NKT275_Q1, BF_Q_NKT275_Q2,
       BF_Q_BC108_Q1, BF_Q_BC108_Q2, BF_Q_OC44, BF_Q_COUNT };

extern const bf_diode_model BF_DIODES[BF_D_COUNT];
extern const bf_bjt_model   BF_BJTS[BF_Q_COUNT];

/* ---- Port runtime descriptors ------------------------------------------- */
enum { BF_PORT_DPAIR = 0, BF_PORT_FA, BF_PORT_BJT_BE, BF_PORT_BJT_BC };

typedef struct {
    uint8_t type;
    /* DPAIR / FA: forward branch (v>0) and reverse branch (v<0) parameters,
     * i(v) = Isf·(e^{v/af} − 1) − Isr·(e^{−v/ar} − 1).  Isr = 0 → no branch. */
    float Isf, af, Isr, ar, iaf, iar;
    /* BJT: Ebers-Moll parameters (BE port carries them; BC port is partner) */
    float Is, kF, kR, iaF, iaR;   /* kF = 1 + 1/βF, kR = 1 + 1/βR */
    float sgn;                    /* +1 NPN, −1 PNP */
} bf_port;

/* ---- Discrete state-space model (float, audio side) ---------------------
 * Working vector z = [u0, 1, x0 … x(N−1), i0 … i(P−1), u2] (packed; u2 only
 * for stages that use the aux input, so other stages skip that column).
 *   p      = Pm · [u0, 1, x, u2]
 *   [x'; y] = Sm · z
 * ------------------------------------------------------------------------- */
#define BF_ZP (BF_MAX_SRC + BF_MAX_STATES)
#define BF_ZS (BF_MAX_SRC + BF_MAX_STATES + BF_MAX_PORTS)

enum { BF_SOLVE_NONE = 0, BF_SOLVE_CLOSED, BF_SOLVE_NEWTON };

typedef struct {
    uint8_t N, P, M, solver, kernel, aux;
    float Pm[BF_MAX_PORTS][BF_ZP];
    float Sm[BF_MAX_STATES + 1][BF_ZS];
    float K[BF_MAX_PORTS][BF_MAX_PORTS];
    bf_port port[BF_MAX_PORTS];
    /* closed-form single port helpers */
    float Kf, invK, lnKIs_f, lnKIs_r;
} bf_dk;

typedef struct {
    float z[BF_ZS];
    float v[BF_MAX_PORTS];        /* port voltages, last sample */
    float vp[BF_MAX_PORTS];       /* port voltages, sample before (predictor) */
} bf_dk_state;

/* Build the discrete model for sample period T with the given knob positions
 * (0..1).  Runs in double; returns false on a singular network. */
bool bf_dk_build(bf_dk *dk, const bf_netlist *nl, double T, const float *knobs);

/* Find the DC operating point (caps open, inductors shorted) for a constant
 * stage input `u0` and write the matching companion states for the model
 * built at period T.  Uses source stepping + damped Newton. */
bool bf_dk_dc_init(bf_dk_state *st, const bf_netlist *nl, double T,
                   const float *knobs, float u0, float *y_dc);

/* Run n samples in place: io[] holds the stage input (u0) on entry and the
 * stage output on return.  aux (may be NULL) supplies u2 per sample. */
void bf_dk_block(const bf_dk *dk, bf_dk_state *st, float *io, const float *aux, int n);

/* Pot taper helper: fraction of total resistance between pin1 and wiper. */
float bf_taper(uint8_t taper, float knob);

#ifdef __cplusplus
}
#endif

#endif /* IFX_BF_CIRCUIT_H */
