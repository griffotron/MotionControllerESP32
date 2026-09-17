#pragma once
#ifndef _RGBBUTTON_h
#define _RGBBUTTON_h

#include <Arduino.h>

// Drives one illuminated push-button consisting of a momentary switch (wired to
// an INPUT_PULLUP pin, so pressed == LOW) and a common-anode RGB LED whose three
// colour channels are active-LOW (writing LOW lights a channel, HIGH turns it off).
//
// The class hides two things the sketch used to do by hand:
//   * debounced edge/hold detection on the switch, and
//   * non-blocking LED effects (solid colour, flashing, single blink),
// so callers only ever deal with high-level state ("flash blue", "was it a short
// press?") and must simply call update() once per loop().
class RgbButton {
public:
    enum class Color : uint8_t { Off, Red, Green, Blue };

    RgbButton(uint8_t buttonPin, uint8_t redPin, uint8_t grnPin, uint8_t bluPin)
        : _buttonPin(buttonPin), _redPin(redPin), _grnPin(grnPin), _bluPin(bluPin) {}

    void begin() {
        pinMode(_buttonPin, INPUT_PULLUP);
        pinMode(_redPin, OUTPUT);
        pinMode(_grnPin, OUTPUT);
        pinMode(_bluPin, OUTPUT);
        _rawState = _debouncedState = (digitalRead(_buttonPin) == LOW);
        setSolid(Color::Off);
    }

    // ---- LED effects (all non-blocking; driven forward by update()) ----

    // Hold a single steady colour.
    void setSolid(Color color) {
        _mode = Mode::Solid;
        _baseColor = color;
        show(color);
    }

    // Blink a colour on/off. toggleMs is how long the LED stays on and off for
    // each half of the cycle, i.e. a full on->off->on cycle takes 2 * toggleMs.
    void setFlash(Color color, uint32_t toggleMs) {
        // Ignore a request that matches the current flash so the rhythm doesn't
        // reset every loop when a caller keeps asking for the same effect.
        if (_mode == Mode::Flash && _baseColor == color && _toggleMs == toggleMs) return;
        _mode = Mode::Flash;
        _baseColor = color;
        _toggleMs = toggleMs;
        _flashOn = true;
        _lastToggle = millis();
        show(color);
    }

    // Momentarily show a colour, then return to the current base effect. Used to
    // acknowledge one-off events (e.g. a single CAN response) without disturbing
    // the underlying state.
    void blinkOnce(Color color, uint32_t durationMs) {
        _blinkActive = true;
        _blinkEnd = millis() + durationMs;
        show(color);
    }

    // ---- Switch input ----

    // Call once per loop(). Debounces the switch and advances LED effects.
    void update() {
        uint32_t now = millis();

        // Debounce: only accept a new stable level after it has held for kDebounceMs.
        bool raw = (digitalRead(_buttonPin) == LOW);
        if (raw != _rawState) {
            _rawState = raw;
            _lastDebounce = now;
        }
        _pressedEdge = _releasedEdge = false;
        if ((now - _lastDebounce) >= kDebounceMs && raw != _debouncedState) {
            _debouncedState = raw;
            if (raw) {
                _pressedEdge = true;
                _pressStart = now;
            } else {
                _releasedEdge = true;
                _lastPressDurationMs = now - _pressStart;
            }
        }

        // Advance whichever LED effect is active.
        if (_blinkActive) {
            if ((int32_t)(now - _blinkEnd) >= 0) {
                _blinkActive = false;
                refreshBase();
            }
        } else if (_mode == Mode::Flash) {
            if ((now - _lastToggle) >= _toggleMs) {
                _lastToggle = now;
                _flashOn = !_flashOn;
                show(_flashOn ? _baseColor : Color::Off);
            }
        }
    }

    bool pressed()  const { return _pressedEdge; }   // true for one update() on press
    bool released() const { return _releasedEdge; }  // true for one update() on release
    bool isDown()   const { return _debouncedState; }
    uint32_t heldForMs() const { return _debouncedState ? (millis() - _pressStart) : 0; }
    uint32_t lastPressDurationMs() const { return _lastPressDurationMs; } // valid after released()

private:
    enum class Mode : uint8_t { Solid, Flash };

    void show(Color c) {
        digitalWrite(_redPin, c == Color::Red   ? LOW : HIGH);
        digitalWrite(_grnPin, c == Color::Green ? LOW : HIGH);
        digitalWrite(_bluPin, c == Color::Blue  ? LOW : HIGH);
    }

    // Restore the LED to whatever the base effect wants after a one-shot blink.
    void refreshBase() {
        show((_mode == Mode::Flash && !_flashOn) ? Color::Off : _baseColor);
    }

    const uint8_t _buttonPin, _redPin, _grnPin, _bluPin;
    static const uint32_t kDebounceMs = 25;

    // Switch state
    bool _rawState = false, _debouncedState = false;
    bool _pressedEdge = false, _releasedEdge = false;
    uint32_t _lastDebounce = 0, _pressStart = 0, _lastPressDurationMs = 0;

    // LED state
    Mode _mode = Mode::Solid;
    Color _baseColor = Color::Off;
    uint32_t _toggleMs = 500, _lastToggle = 0;
    bool _flashOn = true;
    bool _blinkActive = false;
    uint32_t _blinkEnd = 0;
};

#endif
