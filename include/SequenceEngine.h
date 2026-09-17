#pragma once
#include "CANBus.h"

#ifndef _SEQUENCEENGINE_h
#define _SEQUENCEENGINE_h

class SequenceEngine{

    public:
        struct MotionCue {
            MotionCue(uint32_t _moduleId, uint8_t* _data, uint32_t _holdTimeMs){
                moduleId = _moduleId;
                memcpy(data, _data, 8);
                holdTimeMs = _holdTimeMs;
            }
            uint32_t moduleId;
            uint8_t data[8];   //CAN Payload, containing target, pixel id, speed
            uint32_t holdTimeMs; // How long to stay on this step
        };

        struct Sequence {
            char name[32];
            char description[180];
            uint8_t type = 0; // 0 = Generated, 1 = Stored
            std::vector<SequenceEngine::MotionCue> playlist;
            uint8_t speed = 0;
            uint8_t intensity = 0;
            bool loop = false;
            char data[256]; // additional data, e.g. text string for the text display sequences
        };

        struct SequenceManager {
            std::size_t totalSteps;
            Sequence sequence;
            bool loop = true;
            int currentIndex = -1;      // Start before the first index
            uint32_t lastStepMillis = 0;
            bool paused = false;
            bool ready = false;
        };

        
        struct Status {
            bool ready = false;
            bool paused = false;
            bool loop = false;
            const char* name;
            const char* description;
            uint8_t type;
            uint8_t speed;
            uint8_t intensity;
            const char* data;
        };
        

        SequenceEngine(CANBus& can);
        void begin();

        void play();
        void pause();
        void resume();
        void reset();
        void clear();
        void append(SequenceEngine::MotionCue motionCue);
        void loop(bool loop);
        void inject(SequenceEngine::MotionCue motionCue); // runs command immediately, bypassing playlist
        void load(const SequenceEngine::Sequence& sequence);// load whole sequence
        Sequence getCurrentSequence(); // gets the currently loaded sequence, mainly for output as JSON
        Status getStatus();

        // Returns true (and clears the flag) if the play/pause/loop status changed,
        // a new sequence was loaded, or a sequence finished, since the last call.
        // Polled from the main loop to push status over the websocket.
        bool consumeStatusChanged();

    private:
        struct TimerContext {
			SequenceEngine* instance; // "this" pointer
			SequenceManager sequenceMgr; 
			esp_timer_handle_t handle = nullptr;
		};

        // OK, this MutexGuard looks a bit complicated so see note at the top of SequenceEngine.cpp to explain the purpose of the mutex.
        // The reason for using this MutexGuard struct is that the lock will be given up automatically when the function that
        // acquired it exits. That means we don't have to track all the exit routes from a function and ensure the lock is released
        // as that would make the code a bit fragile. Ask me how I know. That's right, I missed one and the whole thing locked up. Whoops!
        struct MutexGuard {
            SemaphoreHandle_t _handle;
            bool _held;

            // portMAX_DELAY for callers that can block (HTTP task methods)
            // 0 for the timer callback (must never block)
            MutexGuard(SemaphoreHandle_t h, TickType_t timeout = portMAX_DELAY) : _handle(h), _held(xSemaphoreTakeRecursive(h, timeout) == pdTRUE) {}

            ~MutexGuard() {
                if(_held){
                    xSemaphoreGiveRecursive(_handle);
                }
            }

            bool locked() const { return _held; }

            // Non-copyable
            MutexGuard(const MutexGuard&) = delete;
            MutexGuard& operator=(const MutexGuard&) = delete;
        };

        TimerContext _ctx;
        CANBus& _can;
        Sequence _pendingSequence;
        SemaphoreHandle_t _mutex;
        Status _lastKnownStatus;
        bool _statusChanged = false;  // set on any status transition, consumed by the websocket pusher

        static void sequenceCallback(void* arg);
        void startEngine();
        void stopEngine();


        void playLocked();
        void pauseLocked();
        void resetLocked();
        void resumeLocked();
};

#endif