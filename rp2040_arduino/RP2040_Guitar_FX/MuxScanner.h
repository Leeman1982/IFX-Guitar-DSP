#pragma once
// =============================================================================
// CD74HC4067  16-channel analog/digital multiplexer driver
// Reads all 6 control channels and debounces buttons + encoder.
// =============================================================================
#include <Arduino.h>
#include "pins.h"
#include "config.h"

// Encoder direction
enum EncoderDir { ENC_NONE = 0, ENC_CW = 1, ENC_CCW = -1 };

class MuxScanner {
public:
    MuxScanner();
    void begin();

    // Call from Core 0 main loop – performs one full 6-channel sweep.
    // Returns true if any state changed this call.
    bool scan();

    // Button events – call after scan(); returns true once per press
    bool distSwitchPressed();
    bool chorusSwitchPressed();
    bool eqSwitchPressed();
    bool encoderPressed();

    // Encoder rotation accumulated since last read; resets to 0 after read
    int8_t encoderDelta();

private:
    void selectChannel(uint8_t ch);
    bool readChannel();

    // Raw state
    bool _rawA,  _rawB;
    bool _rawSwDist, _rawSwChorus, _rawSwEQ, _rawSwEnc;

    // Debounced state
    bool _stateSwDist,  _prevSwDist,  _edgeSwDist;
    bool _stateSwChorus,_prevSwChorus,_edgeSwChorus;
    bool _stateSwEQ,    _prevSwEQ,    _edgeSwEQ;
    bool _stateSwEnc,   _prevSwEnc,   _edgeSwEnc;

    uint32_t _tSwDist,  _tSwChorus, _tSwEQ, _tSwEnc;

    // Gray-code encoder state machine
    uint8_t _encState;      // 2-bit Gray code: A<<1 | B
    int8_t  _encDelta;      // accumulated steps
};
