#pragma once

#ifndef _SEQUENCEGENERATOR_h
#define _SEQUENCEGENERATOR_h

#include <stdint.h>
#include "Config.h"
#include "SequenceEngine.h"
#include "MotorControl.h"

class SequenceGenerator{

    public:
        SequenceGenerator(Config& config, MotorControl& motorCtrl);
        SequenceEngine::Sequence loadFromFile(const char * filePath);

        // Picks a random saved sequence from SEQUENCE_DIR and loads it. Looping is
        // forced off so it plays through once (used by demo mode to keep cycling).
        // Returns a sequence with an empty playlist if there are no saved sequences.
        SequenceEngine::Sequence loadRandomSaved();

        SequenceEngine::Sequence homeAll();
        SequenceEngine::Sequence home(const std::vector<std::array<uint32_t, 2>>& pixels);
        SequenceEngine::Sequence idle();
        SequenceEngine::Sequence flash(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence chase(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence spiral(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence burst(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence verticalWave(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence horizontalWave(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence horizontalSway(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence bouncingBall(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence matrix(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence starfield(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence ripples(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence fire(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence equalizer(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence fireworks(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence snowfall(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence oscilloscope(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence gameOfLife(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence plasma(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence flowField(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence radar(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence smiley(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence snake(uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence scrollText(const char* text, uint8_t speed, uint8_t intensity);
        SequenceEngine::Sequence displayText(const char* text, uint8_t intensity);

        // Reads the system clock and returns a sequence that renders HH:MM using a 7-segment-style 3-wide × 5-tall font, centred on the motor matrix.
        // The final cue holds until the top of the next minute, then the sequence stops — the caller must reload to update the display.
        // Requires NTP (or another time source) to be initialised before calling.
        SequenceEngine::Sequence displayCurrentTime();

        // Build the sequence for an explicit time. holdMs is how long to hold the final cue before the sequence ends (i.e. until the next minute boundary).
        SequenceEngine::Sequence buildTimeSequence(uint8_t hours, uint8_t minutes, uint32_t holdMs);

        // Slap the current sequence into this bad boy and it'll spit out the JSON for it which we can save somewhere
        JsonDocument toJson(const SequenceEngine::Sequence& sequence);

        // Store the current sequence in the file system for faster loading. fileName is the bare name (no path/extension).
        // it's written to /sequences/<fileName>.json. Returns true on a successful write.
        bool saveToFile(const SequenceEngine::Sequence& sequence, const char* fileName);

    private:
        Config& _config;
        MotorControl& _motorContol;
        String _lastRandomSaved;  // avoids picking the same saved sequence twice in a row in demo mode
        int32_t calcTarget(int32_t minTarget, int32_t maxTarget, uint8_t intensity);
        int32_t calcHold(int32_t minHoldTime, int32_t maxHoldTime, uint8_t speed);
        int32_t rampValue(int i, int numCols, int32_t minV, int32_t maxV, float p = 1.7f);

        struct Point {  }; // dir: 0=R, 1=D, 2=L, 3=U
};

#endif