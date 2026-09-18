#include "esp_timer.h"
#include "SequenceEngine.h"
#include <LittleFS.h>
#include "globals.h"
#include "Constants.h"

#define DEBUG true  //set to true for debug output, false for no debug output
#define DEBUG_PRINT if(DEBUG)Serial
#define DEBUG_PRINTF if(DEBUG)printf

/* 
Note: Using the _mutex in here as calls can come in over HTTP which affect the pendingSequence (and other variables)
If this happens at the exact time when the sequenceCallback executes under the timer, then we'll get in a right old pickle
with heap pointers that point nowhere for example. And it'll be so intermittent that it'll be impossible to reproduce.
So, a bit more code, but might as well do it properly while I'm here.
*/

SequenceEngine::SequenceEngine(CANBus& can) : _can(can){
    _mutex = xSemaphoreCreateRecursiveMutex();
    configASSERT(_mutex);   // halt in debug builds if allocation failed
    _ctx.instance = this;
}

void SequenceEngine::append(SequenceEngine::MotionCue motionCue){
    _pendingSequence.playlist.emplace_back(motionCue); 
    Serial.printf("Playlist appended. New size: %d\n", _pendingSequence.playlist.size());
}

void SequenceEngine::inject(SequenceEngine::MotionCue motionCue){
    // bypasses engine and runs immediately
    DEBUG_PRINTF("[INJECTED] ModuleId: %d | Data0: %d \n", motionCue.moduleId, motionCue.data[0]);
    _can.write(motionCue.moduleId, motionCue.data);
}

void SequenceEngine::clear(){
    _pendingSequence.playlist.clear();
    DEBUG_PRINTF("Playlist cleared. Size: %d\n", _pendingSequence.playlist.size());
}

void SequenceEngine::load(const SequenceEngine::Sequence& sequence){
    DEBUG_PRINT.println("New sequence loaded");
    
    Sequence copySequence = sequence; // Heavy copy done outside the lock
    MutexGuard guard(_mutex);  // Will try an acquire a lock, but give up after portMAX_DELAY exceeded - fine on HTTP task
    if(!guard.locked()){
        return;
    } 
    _pendingSequence = std::move(copySequence); // Inside the lock we do a lightweight swap
    playLocked();
}

void SequenceEngine::playLocked() {
    /* using move() below on _pendingSequence as if it's a really big sequence that can stall TCP quite badly.
    Move leaves _pendingSequence empty, but it's not a problem as it'll be assigned a new sequence during load()
    */
    pauseLocked();
    _ctx.sequenceMgr.totalSteps = _pendingSequence.playlist.size();
    _ctx.sequenceMgr.loop = _pendingSequence.loop;
    _ctx.sequenceMgr.sequence = std::move(_pendingSequence);
    resetLocked();
    _ctx.sequenceMgr.ready = (_ctx.sequenceMgr.totalSteps > 0);
    resumeLocked();
    _statusChanged = true;   // a new sequence was loaded/played
}

void SequenceEngine::play(){
    DEBUG_PRINT.println("Sequence PLAY");
    MutexGuard guard(_mutex);
    if(!guard.locked()){
        return;
    } 
    playLocked();
}

void SequenceEngine::pause(){
    DEBUG_PRINT.println("Sequence PAUSED");
    MutexGuard guard(_mutex);
    if(!guard.locked()){
        return;
    }
    pauseLocked();
}

void SequenceEngine::pauseLocked() {
    if (!_ctx.sequenceMgr.paused) {
        _ctx.sequenceMgr.paused = true;
        _statusChanged = true;
    }
}

void SequenceEngine::resume(){
    DEBUG_PRINT.println("Sequence RESUMED");
    MutexGuard guard(_mutex);
    if(!guard.locked()){
        return;
    }
    resumeLocked();
}

void SequenceEngine::resumeLocked(){
    if(_ctx.sequenceMgr.currentIndex == _ctx.sequenceMgr.totalSteps - 1){
        resetLocked(); // jump back to start as we're at the end of the playlist
        _statusChanged = true;
    }

    if(_ctx.sequenceMgr.paused){
        _ctx.sequenceMgr.paused = false;
        _statusChanged = true;

    }
}

void SequenceEngine::reset(){
    DEBUG_PRINT.println("Engine RESET");
    MutexGuard guard(_mutex);
    if(!guard.locked()){
        return;
    }
    resetLocked();
}

void SequenceEngine::resetLocked(){
    _ctx.sequenceMgr.currentIndex = -1;
    _ctx.sequenceMgr.lastStepMillis = 0;
}

void SequenceEngine::loop(bool loop){
    MutexGuard guard(_mutex);
    if(!guard.locked()){
        return;
    }
    if (_ctx.sequenceMgr.loop != loop) {
        _ctx.sequenceMgr.loop = loop;
        _statusChanged = true;
    }
}

void SequenceEngine::begin(){
    reset();
    startEngine();
}

void SequenceEngine::startEngine(){
    if (_ctx.handle != nullptr) {
        esp_timer_stop(_ctx.handle); // Stop and delete existing one if it's there
        esp_timer_delete(_ctx.handle);
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &sequenceCallback,
        .arg = (void*)&_ctx,
        .name = "seq_engine",
    };

    esp_timer_create(&timer_args,  &_ctx.handle);
    esp_timer_start_periodic(_ctx.handle, 1000000 / FRAMES_PER_SECOND); 

    DEBUG_PRINT.println("Engine STARTED");
}

void SequenceEngine::stopEngine(){
    esp_err_t err = esp_timer_stop(_ctx.handle); // Stops the periodic calls
    DEBUG_PRINT.println(err);

    reset();
    DEBUG_PRINT.println("Engine STOPPED");
}

void SequenceEngine::sequenceCallback(void* arg) {
    
    TimerContext* ctx = static_cast<TimerContext*>(arg);
    
    // Non-blocking acquire - if the HTTP task holds it, skip this tick
    MutexGuard guard(ctx->instance->_mutex, 0); // this MutexGuard will automatically release 
    if (!guard.locked()) {
        return;
    }

    SequenceManager& mgr = ctx->sequenceMgr;
    if (mgr.paused || !mgr.ready) {
        return;   // finished/empty sequences exit here
    }

    uint32_t now = millis();

    // If we're holding on a cue, check whether its hold time has expired
    if (mgr.currentIndex >= 0) {
        uint32_t hold = mgr.sequence.playlist[mgr.currentIndex].holdTimeMs;
        if (now - mgr.lastStepMillis < hold) return;
    }

    // Advance and execute, chaining straight through zero-hold cues so a burst of cues (e.g. a full horizontal line) lands in one tick instead of one
    // pixel per tick. Capped at one full pass to avoid spinning forever on an all-zero-hold looping sequence.
    for (std::size_t executed = 0; executed < mgr.totalSteps; executed++) {
        std::size_t next = mgr.currentIndex + 1;
        if (next >= mgr.totalSteps) {
            if (!mgr.loop) {
                mgr.paused = true;   // every future tick exits at the top
                ctx->instance->_statusChanged = true;   // sequence finished
                return;
            }
            next = 0;
        }
        mgr.currentIndex = next;
        mgr.lastStepMillis = now;

        MotionCue& cue = mgr.sequence.playlist[mgr.currentIndex];
        ctx->instance->_can.write(cue.moduleId, cue.data);

        DEBUG_PRINTF("[STEP %d] ModuleId: %d | Holding for: %u ms | NodeId: %u \n", mgr.currentIndex, cue.moduleId, cue.holdTimeMs, cue.data[2]);

        if (cue.holdTimeMs != 0){
            return;
        }
    } 

}

SequenceEngine::Status SequenceEngine::getStatus(){
    MutexGuard guard(_mutex);
    if(!guard.locked()){
        return _lastKnownStatus; // failed to get a lock so just return last known status
    } 

    _lastKnownStatus = {_ctx.sequenceMgr.ready,
        _ctx.sequenceMgr.paused,
        _ctx.sequenceMgr.loop,
        _ctx.sequenceMgr.sequence.name,
        _ctx.sequenceMgr.sequence.description,
        _ctx.sequenceMgr.sequence.type,
        _ctx.sequenceMgr.sequence.speed,
        _ctx.sequenceMgr.sequence.intensity,
        _ctx.sequenceMgr.sequence.data
     };

    return _lastKnownStatus;
}


SequenceEngine::Sequence SequenceEngine::getCurrentSequence(){
    MutexGuard guard(_mutex);
    if(!guard.locked()){
        return Sequence{};
    }

    return _ctx.sequenceMgr.sequence;
}

bool SequenceEngine::consumeStatusChanged(){
    MutexGuard guard(_mutex);
    if(!guard.locked()){
        return false;
    }
    bool changed = _statusChanged;
    _statusChanged = false;
    return changed;
}