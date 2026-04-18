#include "MuxScanner.h"

// Gray-code lookup:  new_state (2 bits) → index into transition table
// Produces +1, -1, or 0 from successive Gray-code readings
static const int8_t ENC_TABLE[16] = {
     0, +1, -1,  0,
    -1,  0,  0, +1,
    +1,  0,  0, -1,
     0, -1, +1,  0
};

MuxScanner::MuxScanner()
    : _rawA(false), _rawB(false),
      _rawSwDist(false), _rawSwChorus(false), _rawSwEQ(false), _rawSwEnc(false),
      _stateSwDist(false),  _prevSwDist(false),  _edgeSwDist(false),
      _stateSwChorus(false),_prevSwChorus(false),_edgeSwChorus(false),
      _stateSwEQ(false),    _prevSwEQ(false),    _edgeSwEQ(false),
      _stateSwEnc(false),   _prevSwEnc(false),   _edgeSwEnc(false),
      _tSwDist(0), _tSwChorus(0), _tSwEQ(0), _tSwEnc(0),
      _encState(0), _encDelta(0)
{}

void MuxScanner::begin()
{
    pinMode(PIN_MUX_S0,  OUTPUT);
    pinMode(PIN_MUX_S1,  OUTPUT);
    pinMode(PIN_MUX_S2,  OUTPUT);
    pinMode(PIN_MUX_S3,  OUTPUT);
    pinMode(PIN_MUX_EN,  OUTPUT);
    pinMode(PIN_MUX_SIG, INPUT_PULLUP);

    digitalWrite(PIN_MUX_EN, LOW);   // enable MUX permanently

    selectChannel(0);
    _encState = 0;
}

void MuxScanner::selectChannel(uint8_t ch)
{
    digitalWrite(PIN_MUX_S0, (ch >> 0) & 1);
    digitalWrite(PIN_MUX_S1, (ch >> 1) & 1);
    digitalWrite(PIN_MUX_S2, (ch >> 2) & 1);
    digitalWrite(PIN_MUX_S3, (ch >> 3) & 1);
    delayMicroseconds(2);   // settling time (MUX tEN max ~100 ns)
}

bool MuxScanner::readChannel()
{
    return digitalRead(PIN_MUX_SIG) == LOW;   // active-LOW (switch to GND)
}

bool MuxScanner::scan()
{
    uint32_t now = millis();
    bool changed = false;

    // ── CH0: Distortion switch ────────────────────────────────────────────
    selectChannel(MUX_CH_SW_DIST);
    bool rd = readChannel();
    if (rd != _stateSwDist) {
        if ((now - _tSwDist) >= SWITCH_DEBOUNCE_MS) {
            _tSwDist      = now;
            _stateSwDist  = rd;
            if (rd) { _edgeSwDist = true; changed = true; }
        }
    } else { _tSwDist = now; }

    // ── CH1: Chorus switch ────────────────────────────────────────────────
    selectChannel(MUX_CH_SW_CHORUS);
    rd = readChannel();
    if (rd != _stateSwChorus) {
        if ((now - _tSwChorus) >= SWITCH_DEBOUNCE_MS) {
            _tSwChorus      = now;
            _stateSwChorus  = rd;
            if (rd) { _edgeSwChorus = true; changed = true; }
        }
    } else { _tSwChorus = now; }

    // ── CH2: EQ switch ────────────────────────────────────────────────────
    selectChannel(MUX_CH_SW_EQ);
    rd = readChannel();
    if (rd != _stateSwEQ) {
        if ((now - _tSwEQ) >= SWITCH_DEBOUNCE_MS) {
            _tSwEQ      = now;
            _stateSwEQ  = rd;
            if (rd) { _edgeSwEQ = true; changed = true; }
        }
    } else { _tSwEQ = now; }

    // ── CH3+4: Rotary encoder (Gray code) ────────────────────────────────
    selectChannel(MUX_CH_ENC_A);
    bool encA = (digitalRead(PIN_MUX_SIG) == HIGH);  // encoder pulls HIGH
    selectChannel(MUX_CH_ENC_B);
    bool encB = (digitalRead(PIN_MUX_SIG) == HIGH);

    uint8_t newState = ((encA ? 1 : 0) << 1) | (encB ? 1 : 0);
    uint8_t idx      = (_encState << 2) | newState;
    int8_t  step     = ENC_TABLE[idx & 0x0F];
    if (step != 0) { _encDelta += step; changed = true; }
    _encState = newState;

    // ── CH5: Encoder push button ─────────────────────────────────────────
    selectChannel(MUX_CH_ENC_SW);
    rd = readChannel();
    if (rd != _stateSwEnc) {
        if ((now - _tSwEnc) >= ENCODER_DEBOUNCE_MS) {
            _tSwEnc      = now;
            _stateSwEnc  = rd;
            if (rd) { _edgeSwEnc = true; changed = true; }
        }
    } else { _tSwEnc = now; }

    return changed;
}

bool MuxScanner::distSwitchPressed()   { bool e = _edgeSwDist;   _edgeSwDist   = false; return e; }
bool MuxScanner::chorusSwitchPressed() { bool e = _edgeSwChorus; _edgeSwChorus = false; return e; }
bool MuxScanner::eqSwitchPressed()     { bool e = _edgeSwEQ;     _edgeSwEQ     = false; return e; }
bool MuxScanner::encoderPressed()      { bool e = _edgeSwEnc;    _edgeSwEnc    = false; return e; }

int8_t MuxScanner::encoderDelta()
{
    int8_t d  = _encDelta;
    _encDelta = 0;
    return d;
}
