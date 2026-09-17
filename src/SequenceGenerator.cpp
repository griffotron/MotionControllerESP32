#include "Config.h"
#include "SequenceEngine.h"
#include "SequenceGenerator.h"
#include "MotorControl.h"
#include "globals.h"
#include <time.h>   // time(), localtime_r() - requires NTP or RTC to be configured
#include <deque>
#include <queue>
#include <esp_system.h>    // esp_random()
#include <esp_task_wdt.h>  // esp_task_wdt_reset()
#include <esp_heap_caps.h> // heap_caps_malloc() for PSRAM allocation
#include "Constants.h"     // SEQUENCE_DIR

#define DEBUG true  //set to true for debug output, false for no debug output
#define DEBUG_PRINT if(DEBUG)Serial
#define DEBUG_PRINTF if(DEBUG)printf

// Allocator that places ArduinoJson documents in PSRAM (this board has 8MB).
// Falls back to internal heap if PSRAM is unavailable. If we end up with some
// whopping great cached sequences with thousands of cues then the 200KB or so of 
// internal heap will be used up and the whole thing crashes. Which would be...er, sub-optimal.
// So this should stop that, meaning it'll only be all other bugs in my code that'll cause it to crash now.
// free() handles both regions on the ESP32.
struct PsramAllocator : ArduinoJson::Allocator {
    void* allocate(size_t size) override {
        void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        return p ? p : malloc(size);
    }
    void deallocate(void* pointer) override {
        free(pointer);
    }
    void* reallocate(void* ptr, size_t size) override {
        void* p = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM);
        return p ? p : realloc(ptr, size);
    }
};
static PsramAllocator g_psramAllocator;

SequenceGenerator::SequenceGenerator(Config& config, MotorControl& motorControl) : _config(config), _motorContol(motorControl){

}

SequenceEngine::Sequence SequenceGenerator::loadFromFile(const char * filePath){
    DEBUG_PRINTF("Loading stored sequence '%s'\n", filePath);
    
    SequenceEngine::Sequence seq = { .name="", .description="", .type = 1, .playlist = {}, .speed = 100, .intensity = 100, .loop = false };

    if(!LittleFS.exists(filePath)){
        DEBUG_PRINTF("Stored sequence '%s' not found\n", filePath);
        return seq;
    }
    
    File file = LittleFS.open(filePath, "r");

    JsonDocument doc(&g_psramAllocator);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        DEBUG_PRINTF("Failed to load JSON. Error: %s\n", error.f_str());
        return seq;
    }
    
    JsonArray sequenceArray = doc["sequence"].as<JsonArray>();
    strlcpy(seq.name, doc["name"] | "Default", sizeof(seq.name));
    strlcpy(seq.description, doc["description"] | "Default", sizeof(seq.description));
    strlcpy(seq.data, doc["data"] | "", sizeof(seq.data));

    seq.loop = doc["loop"].as<bool>();
    seq.playlist.reserve(sequenceArray.size());
    seq.intensity = doc["intensity"].as<uint8_t>();
    seq.speed = doc["speed"].as<uint8_t>();
    seq.type = 1; // Stored

    for(JsonObject cue : sequenceArray) {
        uint8_t data[8] = {0};
        copyArray(cue["d"], data);
        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(cue["m"].as<uint32_t>(), data, cue["h"].as<uint32_t>()));
    }

    DEBUG_PRINTF("Playlist size: %d\n", seq.playlist.size());
    return seq;
}

SequenceEngine::Sequence SequenceGenerator::loadRandomSaved(){
    // Gather the saved sequence file names (matching addHandlerApiSavedSequences).
    std::vector<String> names;
    File dir = LittleFS.open(SEQUENCE_DIR);
    if (dir && dir.isDirectory()) {
        File f = dir.openNextFile();
        while (f) {
            if (!f.isDirectory()) {
                String base = f.name();
                int slash = base.lastIndexOf('/');   // name() may include the path
                if (slash >= 0) base = base.substring(slash + 1);
                if (base.endsWith(".json")) {
                    names.push_back(base.substring(0, base.length() - 5)); // strip ".json"
                }
            }
            f = dir.openNextFile();
        }
    }

    if (names.empty()) {
        DEBUG_PRINTF("No saved sequences to pick from\n");
        return SequenceEngine::Sequence{}; // empty playlist - caller decides what to do
    }

    // Prefer not to repeat the sequence we played last time. Fall back to the full
    // list if that was the only one available.
    std::vector<String> candidates;
    for (const String& n : names) {
        if (n != _lastRandomSaved) candidates.push_back(n);
    }
    std::vector<String>& pool = candidates.empty() ? names : candidates;

    // esp_random() is a hardware RNG, so no seeding needed.
    uint32_t index = esp_random() % pool.size();
    _lastRandomSaved = pool[index];

    char filePath[80];
    snprintf(filePath, sizeof(filePath), "%s/%s.json", SEQUENCE_DIR, pool[index].c_str());
    DEBUG_PRINTF("Demo: picked random saved sequence '%s'\n", pool[index].c_str());

    SequenceEngine::Sequence seq = loadFromFile(filePath);
    seq.loop = false; // demo plays each sequence through once, then moves on
    return seq;
}

bool SequenceGenerator::saveToFile(const SequenceEngine::Sequence& sequence, const char* fileName){
    // Make sure the sequences folder exists before writing into it
    if (!LittleFS.exists(SEQUENCE_DIR)) {
        DEBUG_PRINTF("Creating sequences folder '%s'\n", SEQUENCE_DIR);
        LittleFS.mkdir(SEQUENCE_DIR);
    }

    char filePath[80];
    snprintf(filePath, sizeof(filePath), "%s/%s.json", SEQUENCE_DIR, fileName);
    DEBUG_PRINTF("Saving sequence to '%s'\n", filePath);

    JsonDocument doc = toJson(sequence);

    if (doc.overflowed()) {
        DEBUG_PRINTF("Sequence too large to serialize for '%s'\n", filePath);
        return false;
    }

    File file = LittleFS.open(filePath, "w");
    if (!file) {
        DEBUG_PRINTF("Failed to open '%s' for writing\n", filePath);
        return false;
    }

    size_t written = serializeJson(doc, file);
    file.close();

    DEBUG_PRINTF("Saved %d cues (%u bytes) to '%s'\n", sequence.playlist.size(), written, filePath);
    return written > 0;
}

JsonDocument SequenceGenerator::toJson(const SequenceEngine::Sequence& sequence){
    DEBUG_PRINTF("Outputting current sequence '%s' to JSON\n", sequence.name);
    JsonDocument doc(&g_psramAllocator);
    doc["name"] = sequence.name;
    doc["description"] = sequence.description;
    doc["speed"] = sequence.speed;
    doc["intensity"] = sequence.intensity;
    doc["data"] = sequence.data;
    doc["loop"] = sequence.loop;

    JsonArray seq = doc["sequence"].to<JsonArray>();
    for (const auto& cue : sequence.playlist) {
        JsonObject obj = seq.add<JsonObject>();
        obj["m"] = cue.moduleId;
        obj["h"] = cue.holdTimeMs;
        // CAN payload as a decimal array, e.g. "d": [18,1,2,80,232,3,0,0]
        copyArray(cue.data, 8, obj["d"].to<JsonArray>());
        esp_task_wdt_reset();
    }
    return doc;
}

int32_t SequenceGenerator::calcTarget(int32_t minTarget, int32_t maxTarget, uint8_t intensity){
    int32_t range = maxTarget - minTarget; 
    int32_t intensityAdj = range * intensity;
    int32_t target = minTarget + (intensityAdj / 100);
    return target;
}

int32_t SequenceGenerator::calcHold(int32_t minHoldTime, int32_t maxHoldTime, uint8_t speed){
    int32_t range = maxHoldTime - minHoldTime; 
    int32_t speedAdj = range * speed;
    int32_t holdTime = maxHoldTime - (speedAdj / 100);
    return holdTime;
}

SequenceEngine::Sequence SequenceGenerator::homeAll(){
    DEBUG_PRINTF("Generating home ALL sequence for %d cols and %d rows.\n", _config.getColCount(), _config.getRowCount());
    
    SequenceEngine::Sequence seq = { .name="home-all", .description = "Home all", .type = 0, .playlist = {}, .speed = 100, .intensity = 100, .loop = false };
    seq.playlist.reserve((_config.getColCount() * _config.getRowCount()) * 3);

    for(uint32_t colIndex = 1; colIndex <= _config.getColCount(); colIndex++){
        for(uint32_t rowIndex = 1; rowIndex <= _config.getRowCount(); rowIndex++){
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(colIndex, rowIndex, 1, 50, _config.getHomingTarget(), 50));
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(colIndex, rowIndex, 1, 50, 0, 0));
            seq.playlist.emplace_back(_motorContol.createResetMotorPositionMotionCue(colIndex, rowIndex, 2, 50));
        }
    }
    return seq;
}

SequenceEngine::Sequence SequenceGenerator::home(const std::vector<std::array<uint32_t, 2>>& pixels){
    DEBUG_PRINTF("Generating home sequence for %d pixels.\n", pixels.size());

    SequenceEngine::Sequence seq = { .name="home-pixel", .description = "Home pixel", .type = 0, .playlist = {}, .speed = 100, .intensity = 100, .loop = false };
    seq.playlist.reserve(3 * pixels.size());

    for(std::array<uint32_t, 2> pixelXY : pixels){
        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(pixelXY[0], pixelXY[1], 1, 50, _config.getHomingTarget(), 50));
        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(pixelXY[0], pixelXY[1], 1, 50, 0, 0));
        seq.playlist.emplace_back(_motorContol.createResetMotorPositionMotionCue(pixelXY[0], pixelXY[1], 2, 50));
    }
    
    return seq;
}

SequenceEngine::Sequence SequenceGenerator::idle(){
    DEBUG_PRINTF("Generating idle sequence for %d cols and %d rows.\n", _config.getColCount(), _config.getRowCount());
    
    SequenceEngine::Sequence seq = { .name="idle", .description = "Idle", .type = 0, .playlist = {}, .speed = 50, .intensity = 100, .loop = false };
    seq.playlist.reserve(1);
    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(0, 0, 1, 50, 0, 0));
    return seq;
}

SequenceEngine::Sequence SequenceGenerator::flash(uint8_t speed, uint8_t intensity){
    int32_t target = calcTarget(0, _config.getMaxTarget(), intensity);
    uint32_t hold = calcHold(300, 4000, speed);
    DEBUG_PRINTF("Generating flash %d cols and %d rows. Speed %d, intensity %d, target %d", _config.getColCount(), _config.getRowCount(), speed, intensity, target);

    SequenceEngine::Sequence seq = { .name="flash", .description = "Flash", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };

    seq.playlist.reserve(4);
    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(0, 0, 1, speed, target, hold));
    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(0, 0, 1, speed, 0, hold));
    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(0, 0, 1, speed, target, hold));
    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(0, 0, 1, speed, 0, hold));

    return seq;
}

SequenceEngine::Sequence SequenceGenerator::chase(uint8_t speed, uint8_t intensity){
    int32_t target = calcTarget(0, _config.getMaxTarget(), intensity);
    uint32_t hold = calcHold(10, 300, speed);
    DEBUG_PRINTF("Generating chase %d cols and %d rows. Speed %d, intensity %d, target %d", _config.getColCount(), _config.getRowCount(), speed, intensity, target);

    SequenceEngine::Sequence seq = { .name="chase", .description = "Chase", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((_config.getColCount() * _config.getRowCount()) * 2);

    std::vector<SequenceEngine::MotionCue> playlist;
       for(uint32_t colIndex = 1; colIndex <= _config.getColCount(); colIndex++){
        for(uint32_t rowIndex = 1; rowIndex <= _config.getRowCount(); rowIndex++){
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(colIndex, rowIndex, 1, 80, target, 0));
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(colIndex, rowIndex, 1, 50, 0, hold));
        }
    }

    return seq;
}

// Explosion growing outward from the central two pixels in square-ish rings.
// Each pin in a ring gets its open immediately followed by its half-speed
// close — the motor controllers queue commands per motor and chain them, so
// each pin rises and falls on its own while the playlist just drives the
// blast front outward.
SequenceEngine::Sequence SequenceGenerator::burst(uint8_t speed, uint8_t intensity){
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(25, 800, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = speed;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;  // slow retract trails the blast front

    // The central two pixels — (9,4) and (10,4) on the 18x7 display
    const int cxL = W / 2;
    const int cxR = cxL + 1;
    const int cy  = (H + 1) / 2;

    // Ring number of a pixel: Chebyshev distance to the nearer centre pixel,
    // so ring 0 is exactly the two centres and rings grow as squared-off ovals
    auto ringOf = [&](int x, int y) -> int {
        const int dx = (x < cxL) ? cxL - x : ((x > cxR) ? x - cxR : 0);
        const int dy = (y < cy) ? cy - y : y - cy;
        return std::max(dx, dy);
    };

    // Furthest ring on the display (the corners)
    const int maxRing = std::max(std::max(cxL - 1, W - cxR), std::max(cy - 1, H - cy));

    DEBUG_PRINTF("Generating burst for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms, rings %d\n", W, H, speed, intensity, target, stepMs, maxRing);

    SequenceEngine::Sequence seq = { .name="burst", .description = "Burst", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)W * H * 2 + 8);

    // Frame r: every pin in ring r gets its open immediately followed by its
    // close — the queued close runs the moment the open finishes
    for (int r = 0; r <= maxRing; r++) {
        const size_t frameStart = seq.playlist.size();

        for (int x = 1; x <= W; x++) {
            for (int y = 1; y <= H; y++) {
                if (ringOf(x, y) == r) {
                    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, y, 1, OPEN_SPEED, target, 0));
                    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, y, 1, CLOSE_SPEED, 0, 0));
                }
            }
        }

        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = stepMs;
    }

    DEBUG_PRINTF("Burst: %d cues generated\n", seq.playlist.size());
    return seq;
}

SequenceEngine::Sequence SequenceGenerator::spiral(uint8_t speed, uint8_t intensity){
    int32_t target = calcTarget(0, _config.getMaxTarget(), intensity);
    uint32_t hold = calcHold(1, 300, speed);
    DEBUG_PRINTF("Generating spiral wave for %d cols and %d rows. Speed %d, intensity %d, target %d, hold %d", _config.getColCount(), _config.getRowCount(), speed, intensity, target, hold);

    SequenceEngine::Sequence seq = { .name="spiral", .description = "Spiral", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    std::vector<SequenceEngine::MotionCue> playlist;

    int xMin = 1, xMax = _config.getColCount();
    int yMin = 1, yMax = _config.getRowCount();
    int totalPixels = _config.getColCount() * _config.getRowCount();

    seq.playlist.reserve(totalPixels * 2);
    //Point a = {1, 1, 0}; 

    uint32_t count = 0;
    uint32_t x = 1;
    uint32_t y = 1;
    uint32_t dir = 0;

    while (count < (totalPixels)) {
        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, y, 1, speed, target, 0));
        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, y, 1, speed, 0, hold));              

        if (dir == 0) { // Right
            if (x < xMax) x++; 
            else { dir = 1; y++; yMin++; } // Hit wall, turn Down, shrink top
        } else if (dir == 1) { // Down
            if (y < yMax) y++;
            else { dir = 2; x--; xMax--; } // Hit wall, turn Left, shrink right
        } else if (dir == 2) { // Left
            if (x > xMin) x--;
            else { dir = 3; y--; yMax--; } // Hit wall, turn Up, shrink bottom
        } else if (dir == 3) { // Up
            if (y > yMin) y--;
            else { dir = 0; x++; xMin++; } // Hit wall, turn Right, shrink left
        }
        count++;
    }


    return seq;
}

SequenceEngine::Sequence SequenceGenerator::verticalWave(uint8_t speed, uint8_t intensity){
    int32_t target = calcTarget(0, _config.getMaxTarget(), intensity);
    uint32_t hold = calcHold(25, 800, speed);
    DEBUG_PRINTF("Generating vertical wave for %d cols and %d rows. Speed %d, intensity %d, target %d, hold %d", _config.getColCount(), _config.getRowCount(), speed, intensity, target, hold);

    SequenceEngine::Sequence seq = { .name="vwave", .description = "Vertical wave", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((_config.getColCount() * _config.getRowCount()) * 2);

    std::vector<SequenceEngine::MotionCue> playlist;
    for(uint32_t rowIndex = 1; rowIndex <= _config.getRowCount(); rowIndex++){
        for(uint32_t colIndex = 1; colIndex <= _config.getColCount(); colIndex++){
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(colIndex, rowIndex, 1, speed, target, 0));
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(colIndex, rowIndex, 1, 10, 0, colIndex == _config.getColCount() ? hold : 0));
        }
    }

    return seq;
}

SequenceEngine::Sequence SequenceGenerator::horizontalWave(uint8_t speed, uint8_t intensity){
    int32_t target = calcTarget(0, _config.getMaxTarget(), intensity);
    uint32_t hold = calcHold(25, 800, speed);

    DEBUG_PRINTF("Generating horizontal wave for %d cols and %d rows. Speed %d, intensity %d, hold %d", _config.getColCount(), _config.getRowCount(), speed, intensity, hold);

    SequenceEngine::Sequence seq = { .name="hwave", .description = "Horizontal wave", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((_config.getColCount() * _config.getRowCount()) * 2);

    for(uint32_t colIndex = 1; colIndex <= _config.getColCount(); colIndex++){
        for(uint32_t rowIndex = 1; rowIndex <= _config.getRowCount(); rowIndex++){
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(colIndex, rowIndex, 1, speed, target, 0));
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(colIndex, rowIndex, 1, 10, 0, rowIndex == _config.getRowCount() ? hold : 0));
        }
    }

    return seq;
}

// Ease-in ramp: differences between consecutive columns grow as i grows.
// Higher p = flatter start, steeper finish. i must be in [0 .. numCols-1].
int32_t SequenceGenerator::rampValue(int i, int numCols, int32_t minV, int32_t maxV, float p) {
    const float t = (float)i / (float)(numCols - 1);
    return minV + (int32_t)((float)(maxV - minV) * powf(t, p) + 0.5f);  // +0.5f rounds to nearest
}

SequenceEngine::Sequence SequenceGenerator::horizontalSway(uint8_t speed, uint8_t intensity){
    uint32_t hold = calcHold(1000, 5000, speed);

    DEBUG_PRINTF("Generating horizontal sway for %d cols and %d rows. Speed %d, intensity %d, hold %d", _config.getColCount(), _config.getRowCount(), speed, intensity, hold);

    SequenceEngine::Sequence seq = { .name="hsway", .description = "Sway", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };

    if (_config.getColCount() == 0 || _config.getRowCount() == 0)
        return seq;

    seq.playlist.reserve((_config.getColCount() * _config.getRowCount()) * 2);

    uint32_t minTarget = 0;
    uint32_t maxTarget = _config.getMaxTarget();

    for(uint32_t colIndex = 1; colIndex <= _config.getColCount(); colIndex++){
        uint32_t target = rampValue(colIndex - 1, _config.getColCount(), minTarget, _config.getMaxTarget());
        for(uint32_t rowIndex = 1; rowIndex <= _config.getRowCount(); rowIndex++){
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(colIndex, rowIndex, 1, speed, target, 0));
        }
    }
    seq.playlist.back().holdTimeMs = hold;

    for(uint32_t colIndex = (_config.getColCount()); colIndex >= 1; colIndex--){
        uint32_t target = rampValue(_config.getColCount() - colIndex, _config.getColCount(), minTarget, _config.getMaxTarget());
        for(uint32_t rowIndex = _config.getRowCount(); rowIndex >= 1; rowIndex--){
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(colIndex, rowIndex, 1, speed, target, 0));
        }
    }
    seq.playlist.back().holdTimeMs = hold;

    return seq;
}



// ---------------------------------------------------------------------------
// Clock display bits
// ---------------------------------------------------------------------------
// Quite a lot going on here, so strap in!
// 
// Layout on an 18-wide × 7-tall matrix (1-indexed cols / rows):
//
//   col:  [1 2 3]  4  [5 6 7]  8   9  [10 11 12] 13  [14 15 16] 17  18
//          └─D1──┘ │  └─D2──┘  │   │   └──D3───┘  │   └──D4───┘  │  unused
//          H tens  gap H units  gap colon  M tens  gap   M units  gap
//
//   rows 1 and 7 are unused padding; display rows 2–6 hold the 5-row digits.
//
// Each digit is a 3-column × 5-row 7-segment-style bitmap.
// "#" = motor extends to maxTarget (on), " " = motor stays at 0 (off).
//
//   0:### 1: #  2:###  3:###  4:# #  5:###  6:###  7:###  8:###  9:###
//     # #    #      #      #    # #    #      #        #    # #    # #
//     # #    #    ###    ###    ###    ###    ###      #    ###    ###
//     # #    #    #        #      #      #    # #      #    # #      #
//     ###    #    ###    ###      #    ###    ###      #    ###    ###

// DIGIT_FONT[digit][row 0-4][col 0-2]   true → motor ON (maxTarget)
static const bool DIGIT_FONT[10][5][3] = {
    { {1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1} }, // 0
    { {0,1,0},{0,1,0},{0,1,0},{0,1,0},{0,1,0} }, // 1
    { {1,1,1},{0,0,1},{1,1,1},{1,0,0},{1,1,1} }, // 2
    { {1,1,1},{0,0,1},{1,1,1},{0,0,1},{1,1,1} }, // 3
    { {1,0,1},{1,0,1},{1,1,1},{0,0,1},{0,0,1} }, // 4
    { {1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1} }, // 5
    { {1,1,1},{1,0,0},{1,1,1},{1,0,1},{1,1,1} }, // 6
    { {1,1,1},{0,0,1},{0,0,1},{0,0,1},{0,0,1} }, // 7
    { {1,1,1},{1,0,1},{1,1,1},{1,0,1},{1,1,1} }, // 8
    { {1,1,1},{1,0,1},{1,1,1},{0,0,1},{1,1,1} }, // 9
};

// COLON_FONT[row 0-4]  — two static dots, one column wide
static const bool COLON_FONT[5] = { false, true, false, true, false };

// ---------------------------------------------------------------------------
// Scroll-text font  (3 cols × 5 rows, uppercase A–Z + punctuation)
// SCROLL_ALPHA[letter 0=A..25=Z][row 0-4][col 0-2]  true = pin extended
// ---------------------------------------------------------------------------
static const bool SCROLL_ALPHA[26][5][3] = {
    { {0,1,0},{1,0,1},{1,1,1},{1,0,1},{1,0,1} }, // A
    { {1,1,0},{1,0,1},{1,1,0},{1,0,1},{1,1,0} }, // B
    { {1,1,1},{1,0,0},{1,0,0},{1,0,0},{1,1,1} }, // C
    { {1,1,0},{1,0,1},{1,0,1},{1,0,1},{1,1,0} }, // D
    { {1,1,1},{1,0,0},{1,1,0},{1,0,0},{1,1,1} }, // E
    { {1,1,1},{1,0,0},{1,1,0},{1,0,0},{1,0,0} }, // F
    { {1,1,1},{1,0,0},{1,0,1},{1,0,1},{1,1,1} }, // G
    { {1,0,1},{1,0,1},{1,1,1},{1,0,1},{1,0,1} }, // H
    { {1,1,1},{0,1,0},{0,1,0},{0,1,0},{1,1,1} }, // I
    { {1,1,1},{0,0,1},{0,0,1},{1,0,1},{1,1,1} }, // J
    { {1,0,1},{1,1,0},{1,0,0},{1,1,0},{1,0,1} }, // K
    { {1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,1,1} }, // L
    { {1,0,1},{1,1,1},{1,0,1},{1,0,1},{1,0,1} }, // M
    { {1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1} }, // N  (has to be lower case)
    { {1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1} }, // O
    { {1,1,1},{1,0,1},{1,1,1},{1,0,0},{1,0,0} }, // P
    { {1,1,1},{1,0,1},{1,0,1},{1,1,1},{0,0,1} }, // Q
    { {1,1,1},{1,0,1},{1,1,0},{1,0,1},{1,0,1} }, // R
    { {1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1} }, // S
    { {1,1,1},{0,1,0},{0,1,0},{0,1,0},{0,1,0} }, // T
    { {1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1} }, // U
    { {1,0,1},{1,0,1},{1,0,1},{1,1,1},{0,1,0} }, // V
    { {1,0,1},{1,0,1},{1,0,1},{1,1,1},{1,0,1} }, // W
    { {1,0,1},{1,0,1},{0,1,0},{1,0,1},{1,0,1} }, // X
    { {1,0,1},{1,0,1},{1,1,1},{0,1,0},{0,1,0} }, // Y
    { {1,1,1},{0,0,1},{0,1,0},{1,0,0},{1,1,1} }, // Z
};

// Punctuation bitmaps (3 cols × 5 rows)
static const bool SCROLL_SPACE[5][3]  = { {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0} };
static const bool SCROLL_EXCL[5][3]   = { {0,1,0},{0,1,0},{0,1,0},{0,0,0},{0,1,0} };
static const bool SCROLL_QUES[5][3]   = { {1,1,1},{0,0,1},{0,1,1},{0,0,0},{0,1,0} };
static const bool SCROLL_PERIOD[5][3] = { {0,0,0},{0,0,0},{0,0,0},{0,1,0},{0,1,0} };
static const bool SCROLL_COMMA[5][3]  = { {0,0,0},{0,0,0},{0,0,0},{0,1,0},{1,0,0} };
static const bool SCROLL_MINUS[5][3]  = { {0,0,0},{0,0,0},{1,1,1},{0,0,0},{0,0,0} };
static const bool SCROLL_PLUS[5][3]   = { {0,1,0},{0,1,0},{1,1,1},{0,1,0},{0,1,0} };
static const bool SCROLL_COLON2[5][3] = { {0,0,0},{0,1,0},{0,0,0},{0,1,0},{0,0,0} };
static const bool SCROLL_SLASH[5][3]  = { {0,0,1},{0,0,1},{0,1,0},{1,0,0},{1,0,0} };

// Returns a pointer to the first of the 5 rows of the 3-col bitmap for character c.
// Unrecognised characters map to blank (space).
static auto getScrollCharBitmap(char c) -> const bool (*)[3] {
    c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    if (c >= 'A' && c <= 'Z') return SCROLL_ALPHA[c - 'A'];
    if (c >= '0' && c <= '9') return DIGIT_FONT [c - '0'];
    switch (c) {
        case '!': return SCROLL_EXCL;
        case '?': return SCROLL_QUES;
        case '.': return SCROLL_PERIOD;
        case ',': return SCROLL_COMMA;
        case '-': return SCROLL_MINUS;
        case '+': return SCROLL_PLUS;
        case ':': return SCROLL_COLON2;
        case '/': return SCROLL_SLASH;
        default:  return SCROLL_SPACE;
    }
}

// ---------------------------------------------------------------------------

SequenceEngine::Sequence SequenceGenerator::displayCurrentTime() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    const uint8_t hours   = static_cast<uint8_t>(timeinfo.tm_hour);
    const uint8_t minutes = static_cast<uint8_t>(timeinfo.tm_min);
    const uint8_t seconds = static_cast<uint8_t>(timeinfo.tm_sec);

    // Hold the display until the top of the next minute.
    // At second 0 this is 60 s; at second 59 it is 1 s.
    const uint32_t holdMs = (60u - static_cast<uint32_t>(seconds)) * 1000u;

    DEBUG_PRINTF("Clock: generating %02u:%02u, hold %u ms\n", hours, minutes, holdMs);
    return buildTimeSequence(hours, minutes, holdMs);
}

SequenceEngine::Sequence SequenceGenerator::buildTimeSequence(
        uint8_t hours, uint8_t minutes, uint32_t holdMs) {

    const int cols  = static_cast<int>(_config.getColCount());   // 18
    const int rows  = static_cast<int>(_config.getRowCount());   // 7
    const int32_t maxTgt = static_cast<int32_t>(_config.getMaxTarget());

    // Fixed font dimensions
    static constexpr int DIGIT_W = 3;
    static constexpr int DIGIT_H = 5;
    // Total content width:
    //   D1(3) + gap(1) + D2(3) + gap(1) + colon(1) + gap(1) + D3(3) + gap(1) + D4(3) = 17
    static constexpr int DISPLAY_W = DIGIT_W * 4 + 5;  // 17

    // Guard: matrix must be large enough to hold the display
    if (cols < DISPLAY_W || rows < DIGIT_H) {
        DEBUG_PRINTF("Clock: matrix %dx%d too small (need %dx%d)\n",
                     cols, rows, DISPLAY_W, DIGIT_H);
        SequenceEngine::Sequence empty;
        strlcpy(empty.name, "clock", sizeof(empty.name));
        return empty;
    }

    // Top-left corner of the display region (1-indexed).
    // Integer division floors, so any remainder column lands on the right edge.
    // 18-wide: startX = (18-17)/2 + 1 = 1  →  cols 1–17 active, col 18 unused.
    //  7-tall: startY = ( 7- 5)/2 + 1 = 2  →  rows 2–6  active, rows 1 & 7 unused.
    const int startX = (cols - DISPLAY_W) / 2 + 1;
    const int startY = (rows - DIGIT_H)   / 2 + 1;

    // Absolute column positions of each display element (1-indexed).
    // Offsets within DISPLAY_W: D1=0, gap=3, D2=4, gap=7, colon=8, gap=9, D3=10, gap=13, D4=14
    const int D1_X = startX;       // H tens  : cols [D1_X   .. D1_X+2]
    const int D2_X = startX + 4;   // H units : cols [D2_X   .. D2_X+2]
    const int COLON_X = startX + 8;   // colon   : col   COLON_X
    const int D3_X = startX + 10;  // M tens  : cols [D3_X   .. D3_X+2]
    const int D4_X = startX + 14;  // M units : cols [D4_X   .. D4_X+2]

    // Split into individual digits
    const uint8_t h1 = hours   / 10;
    const uint8_t h2 = hours   % 10;
    const uint8_t m1 = minutes / 10;
    const uint8_t m2 = minutes % 10;

    SequenceEngine::Sequence seq = {
        .name = "clock",
        .description = "Clock",
        .type = 0, 
        .speed = 100,
        .intensity = 100,
        .loop = false
    };

    const uint32_t totalPixels = static_cast<uint32_t>(cols * rows);
    seq.playlist.reserve(totalPixels);

    // Emit one cue per pixel in left-to-right, top-to-bottom raster order.
    // All cues have holdTimeMs = 0 (fire immediately at ~50 fps) except the
    // very last cue, which carries holdMs so the engine waits at the minute
    // boundary before stopping.
    uint32_t cueIndex = 0;

    for (int y = 1; y <= rows; ++y) {
        for (int x = 1; x <= cols; ++x) {

            bool on = false;

            // Is this pixel inside the 5-row vertical display band?
            const int digitRow = y - startY;   // 0-indexed within the 5 digit rows
            if (digitRow >= 0 && digitRow < DIGIT_H) {

                if (x >= D1_X && x < D1_X + DIGIT_W) on = DIGIT_FONT[h1][digitRow][x - D1_X];
                else if (x >= D2_X && x < D2_X + DIGIT_W) on = DIGIT_FONT[h2][digitRow][x - D2_X];
                else if (x == COLON_X) on = COLON_FONT[digitRow];
                else if (x >= D3_X && x < D3_X + DIGIT_W) on = DIGIT_FONT[m1][digitRow][x - D3_X];
                else if (x >= D4_X && x < D4_X + DIGIT_W) on = DIGIT_FONT[m2][digitRow][x - D4_X];
                // Gap columns (startX+6, startX+8) and unused border columns
                // leave 'on' as false → motor returns to 0.
            }

            const int32_t  target  = on ? maxTgt : 0;
            const uint32_t cueHold = (++cueIndex == totalPixels) ? holdMs : 0u;

            seq.playlist.emplace_back(
                _motorContol.createMoveMotionCue(
                    static_cast<uint32_t>(x),
                    static_cast<uint32_t>(y),
                    /*priority=*/ 1,
                    /*speed=*/    80,
                    target,
                    cueHold
                )
            );
        }
    }

    return seq;
}

// --- Purely AI created sequences below ---


SequenceEngine::Sequence SequenceGenerator::bouncingBall(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(50, 500, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    DEBUG_PRINTF("Generating bouncing ball for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "bounce", .description = "Bounce", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };

    // The ball is 2x2 with (bx, by) as its top-left pixel, so valid positions are
    // [1 .. W-1] x [1 .. H-1]. Need at least a 4x4 grid for an interior start.
    if (W < 4 || H < 4) {
        DEBUG_PRINTF("Bouncing ball: matrix %dx%d too small\n", W, H);
        return seq;
    }

    // Random start away from the walls. Wall positions moving outward are
    // transient states the orbit never revisits, so an interior start
    // guarantees the ball eventually returns to this exact state and the
    // playlist loops back to its first frame seamlessly.
    int bx = 2 + (int)(esp_random() % (uint32_t)(W - 3));
    int by = 2 + (int)(esp_random() % (uint32_t)(H - 3));
    int dx = (esp_random() & 1) ? 1 : -1;
    int dy = (esp_random() & 1) ? 1 : -1;

    const int startBx = bx, startBy = by, startDx = dx, startDy = dy;

    // Full period is lcm(2*(W-2), 2*(H-2)) steps; this cap is >= that
    const int maxSteps = 4 * (W - 2) * (H - 2);
    seq.playlist.reserve((size_t)maxSteps * 6 + 8);

    // Clear the display, then draw the ball at its starting position and hold
    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(0, 0, 1, 80, 0, 0));
    for (int ox = 0; ox < 2; ++ox)
        for (int oy = 0; oy < 2; ++oy)
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(bx + ox, by + oy, 1, 80, target, 0));
    seq.playlist.back().holdTimeMs = stepMs;

    for (int step = 0; step < maxSteps; ++step) {
        esp_task_wdt_reset();

        // Bounce: flip a component if the next move would push that edge off the grid
        if (bx + dx < 1 || bx + dx > W - 1) dx = -dx;
        if (by + dy < 1 || by + dy > H - 1) dy = -dy;

        const int nx = bx + dx;
        const int ny = by + dy;

        // Delta render: retract pixels the ball is leaving, extend ones it enters
        for (int ox = 0; ox < 2; ++ox) {
            for (int oy = 0; oy < 2; ++oy) {
                const int px = bx + ox, py = by + oy;
                if (px < nx || px > nx + 1 || py < ny || py > ny + 1)
                    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(px, py, 1, 80, 0, 0));
            }
        }
        for (int ox = 0; ox < 2; ++ox) {
            for (int oy = 0; oy < 2; ++oy) {
                const int px = nx + ox, py = ny + oy;
                if (px < bx || px > bx + 1 || py < by || py > by + 1)
                    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(px, py, 1, 80, target, 0));
            }
        }
        seq.playlist.back().holdTimeMs = stepMs;

        bx = nx;
        by = ny;

        // Back at the starting state — the playlist now wraps cleanly
        if (bx == startBx && by == startBy && dx == startDx && dy == startDy) break;
    }

    DEBUG_PRINTF("Bouncing ball: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Matrix digital rain
// ---------------------------------------------------------------------------
// Streaks fall from the top of the display to the bottom in random columns,
// as made famous by The Matrix. Each pin gets exactly two cues as a streak
// passes: a fast open when the head arrives, then a half-speed close as the
// head moves on — the slow mechanical retraction is what forms the fading
// tail. One streak per column at a time; streaks spawn at random during the
// spawn window, then the last ones drain off the bottom so the display ends
// empty and the sequence wraps cleanly when looped.

SequenceEngine::Sequence SequenceGenerator::matrix(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(60, 500, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = 80;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;  // slow retract forms the tail
    const int     SPAWN_PCT   = 5;               // spawn chance per free column per frame
    const int     spawnFrames = 60;
    const int     drainFrames = H + 2;           // last streaks reach and clear the bottom row
    const int     totalFrames = spawnFrames + drainFrames;

    DEBUG_PRINTF("Generating matrix rain for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "matrix", .description = "Matrix", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)totalFrames * W + 64);

    // One streak per column: row of the head pixel, 0 = no active streak
    std::vector<int> head((size_t)W, 0);

    for (int frame = 0; frame < totalFrames; ++frame) {
        esp_task_wdt_reset();

        const size_t frameStart = seq.playlist.size();

        for (int c = 0; c < W; ++c) {
            bool opened = false;

            if (head[c] > 0) {
                // Close the pin the head is leaving; the half-speed retract is
                // still in progress while the head lights up the rows below
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(c + 1, head[c], 1, CLOSE_SPEED, 0, 0));
                head[c]++;
                if (head[c] > H) head[c] = 0;  // streak has left the screen
                else opened = true;
            }
            else if (frame < spawnFrames && (int)(esp_random() % 100u) < SPAWN_PCT) {
                head[c] = 1;  // new streak enters at the top
                opened = true;
            }

            if (opened)
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(c + 1, head[c], 1, OPEN_SPEED, target, 0));
        }

        // Attach the per-frame hold to the last cue; no-op if nothing happened
        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = stepMs;
        else
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(1, 1, 1, OPEN_SPEED, 0, stepMs));
    }

    DEBUG_PRINTF("Matrix rain: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Star field
// ---------------------------------------------------------------------------
// Twinkling stars: pixels pop open quickly to a random brightness, linger
// for a few frames, then fade out with a half-speed close. Only one star
// opens at a time, with a random gap between openings. Spawning stops near
// the end so the sky empties and the sequence wraps cleanly when looped.

SequenceEngine::Sequence SequenceGenerator::starfield(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(80, 600, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = speed;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;   // slow fade-out
    const int     MIN_GAP     = 1;                // frames between star openings
    const int     MAX_GAP     = 4;
    const int     MIN_LIFE    = 4;                // frames a star stays lit
    const int     MAX_LIFE    = 20;
    const int     spawnFrames = 80;
    const int     totalFrames = spawnFrames + MAX_LIFE + 1;  // let the last stars fade

    DEBUG_PRINTF("Generating star field for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "starfield", .description = "Star Field", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)totalFrames * 8 + 64);

    // Frames of life left per pixel: life[(x-1)*H + (y-1)], 0 = dark
    std::vector<uint8_t> life((size_t)W * H, 0);
    int nextSpawnIn = 1;  // first star opens on the first frame

    for (int frame = 0; frame < totalFrames; ++frame) {
        esp_task_wdt_reset();

        const size_t frameStart = seq.playlist.size();

        // Fade out stars whose life has expired
        for (int x = 1; x <= W; ++x) {
            for (int y = 1; y <= H; ++y) {
                uint8_t& l = life[(size_t)(x - 1) * H + (y - 1)];
                if (l > 0 && --l == 0)
                    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, y, 1, CLOSE_SPEED, 0, 0));
            }
        }

        // Open at most one new star per frame, then wait a random gap
        if (frame < spawnFrames && --nextSpawnIn <= 0) {
            std::vector<int> dark;
            dark.reserve((size_t)W * H);
            for (int i = 0; i < W * H; ++i)
                if (life[i] == 0) dark.push_back(i);

            if (!dark.empty()) {
                const int i = dark[esp_random() % dark.size()];
                const int x = i / H + 1;
                const int y = i % H + 1;
                life[i] = (uint8_t)(MIN_LIFE + esp_random() % (MAX_LIFE - MIN_LIFE + 1));
                // Vary the extension so stars twinkle at different brightnesses
                const int32_t brightness = target * (int32_t)(60 + esp_random() % 41) / 100;
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, y, 1, OPEN_SPEED, brightness, 0));
            }
            nextSpawnIn = MIN_GAP + (int)(esp_random() % (uint32_t)(MAX_GAP - MIN_GAP + 1));
        }

        // Attach the per-frame hold to the last cue; no-op if nothing happened
        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = stepMs;
        else
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(1, 1, 1, OPEN_SPEED, 0, stepMs));
    }

    DEBUG_PRINTF("Star field: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Rainfall ripples
// ---------------------------------------------------------------------------
// Raindrops land one at a time at random points on the display. Each drop
// spreads outward as a round ring that loses height as it travels, with the
// fast-open / half-speed-close pattern leaving a soft trail behind the ring.
// A random pause separates the drops.

SequenceEngine::Sequence SequenceGenerator::ripples(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(60, 500, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = speed;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;      // slow retract trails the ring
    const int     RADIUS      = std::max(8, H / 2);  // how far each ripple spreads
    const int     DROPS       = 12;                  // raindrops per run of the sequence
    const int     MIN_GAP     = 2;                   // idle frames between drops
    const int     MAX_GAP     = 8;

    DEBUG_PRINTF("Generating ripples for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "ripples", .description = "Ripples", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)DROPS * (RADIUS + 2) * 16 + 16);

    for (int drop = 0; drop < DROPS; ++drop) {
        esp_task_wdt_reset();

        // Where this raindrop lands; rings past the display edge just clip
        const int cx = 1 + (int)(esp_random() % (uint32_t)W);
        const int cy = 1 + (int)(esp_random() % (uint32_t)H);

        // Ring number: Euclidean distance to the drop point (rounded), so the
        // ripple spreads as a circle rather than a square
        auto ringOf = [&](int x, int y) -> int {
            const float dx = (float)(x - cx);
            const float dy = (float)(y - cy);
            return (int)(sqrtf(dx * dx + dy * dy) + 0.5f);
        };

        // Frame r: every pin in ring r gets its open immediately followed by
        // its close. The motor controllers queue commands per motor and run
        // the next one the moment the previous finishes, so each pin rises
        // and falls on its own while the playlist just drives the wavefront.
        for (int r = 0; r <= RADIUS; ++r) {
            const size_t frameStart = seq.playlist.size();

            // The ring loses height as it spreads out from the centre
            const int32_t ringTarget = target * (RADIUS + 1 - r) / (RADIUS + 1);

            for (int x = 1; x <= W; ++x) {
                for (int y = 1; y <= H; ++y) {
                    if (ringOf(x, y) == r) {
                        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, y, 1, OPEN_SPEED, ringTarget, 0));
                        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, y, 1, CLOSE_SPEED, 0, 0));
                    }
                }
            }

            if (seq.playlist.size() > frameStart)
                seq.playlist.back().holdTimeMs = stepMs;
        }

        // Random pause before the next drop lands (no-op cue carrying the hold)
        const uint32_t gap = (uint32_t)(MIN_GAP + esp_random() % (uint32_t)(MAX_GAP - MIN_GAP + 1));
        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(cx, cy, 1, CLOSE_SPEED, 0, gap * stepMs));
    }

    DEBUG_PRINTF("Ripples: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Fire
// ---------------------------------------------------------------------------
// Flames licking up from the bottom row. A heat field is seeded hot along the
// bottom each frame and propagates upward, cooling as it rises, so the tallest
// pins sit at the base and shrink toward the top. Pin height = heat. Rising
// heat flares up fast; cooling settles slowly. The seed stops near the end so
// the fire dies down to nothing, matching the empty start for a clean loop.

SequenceEngine::Sequence SequenceGenerator::fire(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(40, 200, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t RISE_SPEED  = speed;             // flare up fast
    const uint8_t FALL_SPEED  = RISE_SPEED / 2; // settle down slowly
    const int     COOLING     = 90 / H;         // heat lost per row climbed
    const int     LEVELS      = 5;              // quantise height to reduce jitter
    const int     seedFrames  = 140;
    const int     totalFrames = seedFrames + H + 4;  // let the fire die out

    DEBUG_PRINTF("Generating fire for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "fire", .description = "Fire", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)totalFrames * W * 3 + 64);

    auto at = [&](std::vector<int>& v, int x, int y) -> int& { return v[(size_t)(x - 1) * H + (y - 1)]; };

    std::vector<int> heat((size_t)W * H, 0);     // 0..100, y=1 top, y=H bottom
    std::vector<int> level((size_t)W * H, 0);    // last commanded quantised level

    for (int frame = 0; frame < totalFrames; ++frame) {
        esp_task_wdt_reset();

        const bool seeding = frame < seedFrames;

        // Build the next heat field from the current one
        std::vector<int> next((size_t)W * H, 0);
        for (int x = 1; x <= W; ++x)
            at(next, x, H) = seeding ? (int)(55 + esp_random() % 46) : 0;  // embers along the base

        for (int y = 1; y < H; ++y) {            // every row above the base
            for (int x = 1; x <= W; ++x) {
                // Average the three cells below from the previous frame so heat
                // rises one row per frame
                const int l = at(heat, std::max(1, x - 1), y + 1);
                const int c = at(heat, x, y + 1);
                const int r = at(heat, std::min(W, x + 1), y + 1);
                int h = (l + 2 * c + r) / 4 - (COOLING + (int)(esp_random() % 4));
                at(next, x, y) = h < 0 ? 0 : h;
            }
        }
        heat.swap(next);

        const size_t frameStart = seq.playlist.size();

        for (int x = 1; x <= W; ++x) {
            for (int y = 1; y <= H; ++y) {
                const int lvl = at(heat, x, y) * LEVELS / 101;  // 0..LEVELS-1
                int& cur = at(level, x, y);
                if (lvl == cur) continue;

                const uint8_t spd = (lvl > cur) ? RISE_SPEED : FALL_SPEED;
                cur = lvl;
                const int32_t tgt = target * lvl / (LEVELS - 1);
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, y, 1, spd, tgt, 0));
            }
        }

        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = stepMs;
        else
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(1, 1, 1, RISE_SPEED, 0, stepMs));
    }

    DEBUG_PRINTF("Fire: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Equalizer bars
// ---------------------------------------------------------------------------
// An audio-visualiser: each column is a bar that jumps up fast on a random
// "beat" then falls back under gravity, with a peak-hold cap that floats above
// and slowly sinks. Newly lit pins open fast, cleared pins close at half speed
// for a soft settle. Beats stop near the end so every bar drains to empty for
// a clean loop.

SequenceEngine::Sequence SequenceGenerator::equalizer(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(300, 1000, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = speed;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;
    const int     BEAT_PCT    = 12;    // chance per column per frame of a new beat
    const int     beatFrames  = 140;
    const int     totalFrames = beatFrames + 2 * H + 4;  // let bars and peaks drain

    DEBUG_PRINTF("Generating equalizer for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "equalizer", .description = "Equalizer", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)totalFrames * W + 64);

    std::vector<float> level((size_t)W, 0.0f);   // current bar height in rows
    std::vector<float> peak((size_t)W, 0.0f);     // floating peak-hold height
    std::vector<uint8_t> state((size_t)W * H, 0); // last commanded on/off per pixel

    for (int frame = 0; frame < totalFrames; ++frame) {
        esp_task_wdt_reset();

        const size_t frameStart = seq.playlist.size();

        for (int c = 0; c < W; ++c) {
            // Drive the bar: a beat snaps it up, otherwise gravity pulls it down
            if (frame < beatFrames && (int)(esp_random() % 100u) < BEAT_PCT) {
                const float hit = 2.0f + (float)(esp_random() % (uint32_t)(H - 1));  // 2..H
                if (hit > level[c]) level[c] = hit;
            } else {
                level[c] -= 0.8f;  // gravity
            }
            if (level[c] < 0.0f) level[c] = 0.0f;

            // Peak cap rides the top of the bar, then sinks slowly
            if (level[c] >= peak[c]) peak[c] = level[c];
            else peak[c] -= 0.2f;
            if (peak[c] < 0.0f) peak[c] = 0.0f;

            const int barRows  = (int)(level[c] + 0.5f);
            const int peakRow  = (int)(peak[c]  + 0.5f);  // 0 = no cap

            for (int y = 1; y <= H; ++y) {
                const int fromBottom = H - y + 1;                     // 1 at base
                const bool lit = (fromBottom <= barRows) || (peakRow > 0 && fromBottom == peakRow);

                uint8_t& cur = state[(size_t)c * H + (y - 1)];
                if ((uint8_t)lit == cur) continue;

                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                    c + 1, y, 1, lit ? OPEN_SPEED : CLOSE_SPEED, lit ? target : 0, 0));
                cur = (uint8_t)lit;
            }
        }

        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = stepMs;
        else
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(1, 1, 1, OPEN_SPEED, 0, stepMs));
    }

    DEBUG_PRINTF("Equalizer: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Fireworks
// ---------------------------------------------------------------------------
// One rocket at a time launches from the bottom of a random column, climbing
// with a fading spark trail, then detonates at its apex into an expanding
// ring of sparks (the burst/ripple pattern: fast open, queued half-speed
// close). A random pause separates the launches. Ends empty for a clean loop.

SequenceEngine::Sequence SequenceGenerator::fireworks(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(50, 350, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = speed;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;
    const int     ROCKETS     = 8;
    const int     BLAST_R     = std::max(2, H / 2);  // explosion radius
    const int     MIN_GAP     = 2;                   // idle frames between launches
    const int     MAX_GAP     = 6;

    DEBUG_PRINTF("Generating fireworks for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "fireworks", .description = "Fireworks", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)ROCKETS * (H + BLAST_R) * 12 + 32);

    for (int rocket = 0; rocket < ROCKETS; ++rocket) {
        esp_task_wdt_reset();

        const int lx = 1 + (int)(esp_random() % (uint32_t)W);          // launch column
        const int ay = 1 + (int)(esp_random() % (uint32_t)std::max(1, H / 2));  // apex row (upper half)

        // --- Ascent: a spark climbs from the base to the apex, the pin behind
        //     it closing slowly to leave a fading trail. Trail pins within the
        //     blast radius are left extended rather than closed — a slow close
        //     here would still be retracting as the explosion bursts past it,
        //     so instead they fold straight into the blast and the expanding
        //     rings fade them out.
        for (int y = H; y >= ay; --y) {
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(lx, y, 1, OPEN_SPEED, target, 0));
            if (y < H && (y + 1 - ay) > BLAST_R)  // close the trail pin, unless the blast will take it
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(lx, y + 1, 1, CLOSE_SPEED, 0, 0));
            seq.playlist.back().holdTimeMs = stepMs;
        }

        // --- Detonation: expanding rings of sparks centred on the apex, each
        //     pin popping up and dropping back via queued open+close
        auto ringOf = [&](int x, int y) -> int {
            const float dx = (float)(x - lx);
            const float dy = (float)(y - ay);
            return (int)(sqrtf(dx * dx + dy * dy) + 0.5f);
        };

        for (int r = 0; r <= BLAST_R; ++r) {
            const size_t frameStart = seq.playlist.size();
            const int32_t ringTarget = target * (BLAST_R + 1 - r) / (BLAST_R + 1);  // sparks fade outward

            for (int x = 1; x <= W; ++x) {
                for (int y = 1; y <= H; ++y) {
                    if (ringOf(x, y) == r && !(x == lx && y == ay && r == 0)) {
                        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, y, 1, OPEN_SPEED, ringTarget, 0));
                        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, y, 1, CLOSE_SPEED, 0, 0));
                    }
                }
            }

            // The apex pin is still up from the ascent — drop it as the blast leaves
            if (r == 0)
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(lx, ay, 1, CLOSE_SPEED, 0, 0));

            if (seq.playlist.size() > frameStart)
                seq.playlist.back().holdTimeMs = stepMs;
        }

        // Random pause before the next launch
        const uint32_t gap = (uint32_t)(MIN_GAP + esp_random() % (uint32_t)(MAX_GAP - MIN_GAP + 1));
        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(lx, ay, 1, CLOSE_SPEED, 0, gap * stepMs));
    }

    DEBUG_PRINTF("Fireworks: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Snowfall
// ---------------------------------------------------------------------------
// Flakes drift down random columns and settle into drifts that build up from
// the bottom. Once the drifts have grown, the snow melts away evenly so the
// display ends empty and the sequence wraps cleanly when looped.

SequenceEngine::Sequence SequenceGenerator::snowfall(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(50, 350, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = 80;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;
    const int     SPAWN_PCT   = 45;    // chance per frame of dropping a new flake
    const int     accumFrames = 180;   // keep snowing this long, then melt

    DEBUG_PRINTF("Generating snowfall for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "snowfall", .description = "Snowfall", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)(accumFrames + 6 * H) * W + 64);

    std::vector<int>     pile((size_t)W, 0);       // settled rows per column
    std::vector<int>     flake((size_t)W, 0);      // row of the falling flake, 0 = none
    std::vector<uint8_t> state((size_t)W * H, 0);  // lit per pixel

    // Redraws the display (drifts + falling flakes) via delta cues
    auto render = [&]() {
        const size_t frameStart = seq.playlist.size();
        for (int x = 1; x <= W; ++x) {
            for (int y = 1; y <= H; ++y) {
                const int fromBottom = H - y + 1;
                const bool lit = (fromBottom <= pile[x - 1]) || (flake[x - 1] == y);
                uint8_t& cur = state[(size_t)(x - 1) * H + (y - 1)];
                if ((uint8_t)lit == cur) continue;
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                    x, y, 1, lit ? OPEN_SPEED : CLOSE_SPEED, lit ? target : 0, 0));
                cur = (uint8_t)lit;
            }
        }
        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = stepMs;
        else
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(1, 1, 1, OPEN_SPEED, 0, stepMs));
    };

    // --- Accumulation: snow falls and settles
    for (int frame = 0; ; ++frame) {
        esp_task_wdt_reset();

        // Advance falling flakes; settle any that reach the top of their drift
        for (int c = 0; c < W; ++c) {
            if (flake[c] == 0) continue;
            const int landing = H - pile[c];      // lowest empty row in this column
            if (flake[c] + 1 > landing) { if (pile[c] < H) pile[c]++; flake[c] = 0; }
            else flake[c]++;
        }

        // Drop a new flake into a random column that has room and no flake yet
        if (frame < accumFrames && (int)(esp_random() % 100u) < SPAWN_PCT) {
            const int c = (int)(esp_random() % (uint32_t)W);
            if (pile[c] < H && flake[c] == 0) flake[c] = 1;
        }

        render();

        bool anyFlake = false;
        for (int c = 0; c < W; ++c) if (flake[c]) { anyFlake = true; break; }
        if ((frame >= accumFrames && !anyFlake) || frame > accumFrames + 6 * H) break;
    }

    // --- Melt: drifts sink away evenly until the display is empty
    for (int guard = 0; guard < 6 * H; ++guard) {
        esp_task_wdt_reset();
        bool any = false;
        for (int c = 0; c < W; ++c) {
            if (pile[c] > 0 && (int)(esp_random() % 100u) < 40) pile[c]--;
            if (pile[c] > 0) any = true;
        }
        render();
        if (!any) break;
    }

    DEBUG_PRINTF("Snowfall: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Oscilloscope
// ---------------------------------------------------------------------------
// One lit pin per column traces a scrolling two-harmonic waveform. As the pin
// moves the one it left closes at half speed, leaving a brief phosphor trail.
// Runs exactly one phase cycle so enabling loop scrolls seamlessly.

SequenceEngine::Sequence SequenceGenerator::oscilloscope(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(30, 200, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = 80;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;
    const int     period      = 60;              // frames per full scroll cycle
    const float   centre      = (H + 1) / 2.0f;
    const float   amp         = (H - 1) / 2.0f;

    DEBUG_PRINTF("Generating oscilloscope for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "oscilloscope", .description = "Oscilloscope", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)period * W * 2 + 32);

    auto waveRow = [&](int x, float phase) -> int {
        const float u = (float)(x - 1) / (float)W;
        const float v = 0.6f * sinf(2.0f * (float)M_PI * u * 2.0f + phase)
                      + 0.4f * sinf(2.0f * (float)M_PI * u * 3.0f - phase);
        int row = (int)(centre - amp * v + 0.5f);
        return row < 1 ? 1 : (row > H ? H : row);
    };

    // Seed the "previous" row with the frame just before the wrap, so the cue
    // list forms a closed cycle and looping is seamless
    std::vector<int> lastRow((size_t)W, 0);
    for (int x = 1; x <= W; ++x)
        lastRow[x - 1] = waveRow(x, 2.0f * (float)M_PI * (period - 1) / period);

    for (int frame = 0; frame < period; ++frame) {
        esp_task_wdt_reset();
        const float phase = 2.0f * (float)M_PI * frame / period;
        const size_t frameStart = seq.playlist.size();

        for (int x = 1; x <= W; ++x) {
            const int row = waveRow(x, phase);
            if (row == lastRow[x - 1]) continue;
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, row, 1, OPEN_SPEED, target, 0));
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(x, lastRow[x - 1], 1, CLOSE_SPEED, 0, 0));
            lastRow[x - 1] = row;
        }

        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = stepMs;
        else
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(1, 1, 1, OPEN_SPEED, 0, stepMs));
    }

    DEBUG_PRINTF("Oscilloscope: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Conway's Game of Life
// ---------------------------------------------------------------------------
// Pre-simulates Life on a toroidal (wrap-around) grid from a random seed.
// Births open fast, deaths fade at half speed. Stops early if the colony dies
// out or settles, then clears the display so the loop restarts from the seed.

SequenceEngine::Sequence SequenceGenerator::gameOfLife(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(150, 700, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = 80;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;
    const int     maxGens     = 80;

    DEBUG_PRINTF("Generating game of life for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "life", .description = "Game of Life", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)maxGens * W * H / 2 + 64);

    auto idx = [&](int x, int y) { return (size_t)x * H + y; };  // 0-indexed x,y
    std::vector<uint8_t> cell((size_t)W * H, 0);
    std::vector<uint8_t> state((size_t)W * H, 0);  // physical lit state

    // Random seed (~35% alive)
    for (int i = 0; i < W * H; ++i) cell[i] = (esp_random() % 100u) < 35 ? 1 : 0;

    for (int gen = 0; gen < maxGens; ++gen) {
        esp_task_wdt_reset();

        // Render this generation (delta against the physical state)
        const size_t frameStart = seq.playlist.size();
        for (int x = 0; x < W; ++x) {
            for (int y = 0; y < H; ++y) {
                const uint8_t alive = cell[idx(x, y)];
                uint8_t& cur = state[idx(x, y)];
                if (alive == cur) continue;
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                    x + 1, y + 1, 1, alive ? OPEN_SPEED : CLOSE_SPEED, alive ? target : 0, 0));
                cur = alive;
            }
        }
        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = stepMs;
        else
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(1, 1, 1, OPEN_SPEED, 0, stepMs));

        // Compute the next generation with toroidal wrap
        std::vector<uint8_t> next((size_t)W * H, 0);
        int population = 0, changes = 0;
        for (int x = 0; x < W; ++x) {
            for (int y = 0; y < H; ++y) {
                int n = 0;
                for (int dx = -1; dx <= 1; ++dx)
                    for (int dy = -1; dy <= 1; ++dy) {
                        if (dx == 0 && dy == 0) continue;
                        n += cell[idx((x + dx + W) % W, (y + dy + H) % H)];
                    }
                const uint8_t alive = cell[idx(x, y)];
                const uint8_t live  = (alive && (n == 2 || n == 3)) || (!alive && n == 3) ? 1 : 0;
                next[idx(x, y)] = live;
                population += live;
                if (live != alive) changes++;
            }
        }
        cell.swap(next);
        if (population == 0 || changes == 0) break;  // died out or stabilised
    }

    // Clear the display and hold, so the loop restarts cleanly from the seed
    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(0, 0, 1, CLOSE_SPEED, 0, stepMs * 4));

    DEBUG_PRINTF("Game of life: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Plasma
// ---------------------------------------------------------------------------
// A classic demoscene plasma: pin height is the sum of a few sine waves of x,
// y, x+y and the distance to a point orbiting the display. Heights are
// quantised to a handful of levels to keep the motion meaningful. Runs one
// full time cycle so enabling loop drifts seamlessly.

SequenceEngine::Sequence SequenceGenerator::plasma(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(40, 220, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = 80;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;
    const int     LEVELS      = 5;
    const int     period      = 40;

    DEBUG_PRINTF("Generating plasma for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "plasma", .description = "Plasma", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)period * W * H + 64);

    auto fieldLevel = [&](int x, int y, float t) -> int {
        const float cxp = W / 2.0f + (W / 3.0f) * cosf(t);
        const float cyp = H / 2.0f + (H / 3.0f) * sinf(t);
        const float dx = x - cxp, dy = y - cyp;
        const float v = sinf(x * 0.6f + t)
                      + sinf(y * 0.9f - t)
                      + sinf((x + y) * 0.5f + t)
                      + sinf(sqrtf(dx * dx + dy * dy) * 0.9f - t);
        int lvl = (int)((v + 4.0f) * (LEVELS / 8.0f));
        return lvl < 0 ? 0 : (lvl >= LEVELS ? LEVELS - 1 : lvl);
    };

    // Seed the level buffer with the pre-wrap frame for a seamless loop
    std::vector<int> level((size_t)W * H, 0);
    for (int x = 1; x <= W; ++x)
        for (int y = 1; y <= H; ++y)
            level[(size_t)(x - 1) * H + (y - 1)] = fieldLevel(x, y, 2.0f * (float)M_PI * (period - 1) / period);

    for (int frame = 0; frame < period; ++frame) {
        esp_task_wdt_reset();
        const float t = 2.0f * (float)M_PI * frame / period;
        const size_t frameStart = seq.playlist.size();

        for (int x = 1; x <= W; ++x) {
            for (int y = 1; y <= H; ++y) {
                const int lvl = fieldLevel(x, y, t);
                int& cur = level[(size_t)(x - 1) * H + (y - 1)];
                if (lvl == cur) continue;
                const uint8_t spd = (lvl > cur) ? OPEN_SPEED : CLOSE_SPEED;
                cur = lvl;
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                    x, y, 1, spd, target * lvl / (LEVELS - 1), 0));
            }
        }

        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = stepMs;
        else
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(1, 1, 1, OPEN_SPEED, level[0] * target / (LEVELS - 1), stepMs));
    }

    DEBUG_PRINTF("Plasma: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Flow field
// ---------------------------------------------------------------------------
// Broad diagonal bands of height drift across the grid like wind over a field
// of wheat —- two overlaid travelling waves at different angles. Same quantised
// height + seamless one-cycle loop approach as the plasma.

SequenceEngine::Sequence SequenceGenerator::flowField(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(50, 280, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = 80;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;
    const int     LEVELS      = 5;
    const int     period      = 48;

    DEBUG_PRINTF("Generating flow field for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "flow", .description = "Flow field", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)period * W * H + 64);

    auto fieldLevel = [&](int x, int y, float t) -> int {
        const float v = sinf(x * 0.5f + y * 0.3f - t)
                      + 0.7f * sinf(x * 0.2f - y * 0.55f - 2.0f * t);
        int lvl = (int)((v + 1.7f) * (LEVELS / 3.4f));
        return lvl < 0 ? 0 : (lvl >= LEVELS ? LEVELS - 1 : lvl);
    };

    std::vector<int> level((size_t)W * H, 0);
    for (int x = 1; x <= W; ++x)
        for (int y = 1; y <= H; ++y)
            level[(size_t)(x - 1) * H + (y - 1)] = fieldLevel(x, y, 2.0f * (float)M_PI * (period - 1) / period);

    for (int frame = 0; frame < period; ++frame) {
        esp_task_wdt_reset();
        const float t = 2.0f * (float)M_PI * frame / period;
        const size_t frameStart = seq.playlist.size();

        for (int x = 1; x <= W; ++x) {
            for (int y = 1; y <= H; ++y) {
                const int lvl = fieldLevel(x, y, t);
                int& cur = level[(size_t)(x - 1) * H + (y - 1)];
                if (lvl == cur) continue;
                const uint8_t spd = (lvl > cur) ? OPEN_SPEED : CLOSE_SPEED;
                cur = lvl;
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                    x, y, 1, spd, target * lvl / (LEVELS - 1), 0));
            }
        }

        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = stepMs;
        else
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(1, 1, 1, OPEN_SPEED, level[0] * target / (LEVELS - 1), stepMs));
    }

    DEBUG_PRINTF("Flow field: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Radar sweep
// ---------------------------------------------------------------------------
// A line rotates around the centre of the display, leaving a fading wedge of
// recently-swept pins behind it (fast open on the leading edge, half-speed
// close as each pin's trail life expires). The trailing wedge is pre-seeded so
// the state at the end of one revolution lines up exactly with the start,
// letting the rotation loop continuously with no gap. Each pin is swept once
// per revolution and closes a trail-life later — long before the beam returns —
// so it is never re-touched while still settling.

SequenceEngine::Sequence SequenceGenerator::radar(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(30, 180, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED   = 80;
    const uint8_t CLOSE_SPEED  = OPEN_SPEED / 2;
    const int     stepsPerRev  = 48;
    const int     trailLife    = 12;
    const float   dth          = 2.0f * (float)M_PI / stepsPerRev;
    const float   cx           = (W + 1) / 2.0f;
    const float   cy           = (H + 1) / 2.0f;
    const float   maxR         = sqrtf((W / 2.0f) * (W / 2.0f) + (H / 2.0f) * (H / 2.0f)) + 1.0f;

    DEBUG_PRINTF("Generating radar for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "radar", .description = "Radar", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };
    seq.playlist.reserve((size_t)stepsPerRev * W * 3 + 64);

    std::vector<int>     life((size_t)W * H, 0);
    std::vector<uint8_t> state((size_t)W * H, 0);

    // Stamp the ray at angle th into the life buffer, taking the max so the
    // freshest sweep wins
    auto sweep = [&](float th, int val) {
        for (float r = 0.0f; r <= maxR; r += 0.5f) {
            const int x = (int)(cx + r * cosf(th) + 0.5f);
            const int y = (int)(cy + r * sinf(th) + 0.5f);
            if (x >= 1 && x <= W && y >= 1 && y <= H) {
                int& L = life[(size_t)(x - 1) * H + (y - 1)];
                if (val > L) L = val;
            }
        }
    };

    auto render = [&]() {
        const size_t frameStart = seq.playlist.size();
        for (int x = 1; x <= W; ++x) {
            for (int y = 1; y <= H; ++y) {
                const bool lit = life[(size_t)(x - 1) * H + (y - 1)] > 0;
                uint8_t& cur = state[(size_t)(x - 1) * H + (y - 1)];
                if ((uint8_t)lit == cur) continue;
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                    x, y, 1, lit ? OPEN_SPEED : CLOSE_SPEED, lit ? target : 0, 0));
                cur = (uint8_t)lit;
            }
        }
        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = stepMs;
        else
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(1, 1, 1, OPEN_SPEED, 0, stepMs));
    };

    // Pre-seed the trailing wedge behind the start angle so the first frame
    // already has a full trail and the end of the revolution lines up exactly
    // with the start — the rotation then loops continuously with no gap. The
    // life of the ray swept m frames ago is (trailLife - m + 1), matching what
    // the decay below will have left it at when the revolution wraps.
    for (int m = 1; m <= trailLife; ++m) sweep(-m * dth, trailLife - m + 1);
    render();

    // Sweep exactly one full revolution. Each pin opens as the beam crosses it
    // and closes one trail-life later, long before the beam comes back round.
    for (int f = 0; f < stepsPerRev; ++f) {
        esp_task_wdt_reset();
        for (size_t i = 0; i < life.size(); ++i) if (life[i] > 0) life[i]--;  // age the trail
        sweep(f * dth, trailLife);                                            // current sweep line
        render();
    }

    DEBUG_PRINTF("Radar: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Smiley face
// ---------------------------------------------------------------------------
// A face centred on the display, using the full height for resolution: a round
// outline with two eyes and a mouth that animates from a straight line into a
// smile and back, so it loops as a repeating grin. Only the mouth pins move —
// the outline and eyes are drawn once and held.

SequenceEngine::Sequence SequenceGenerator::smiley(uint8_t speed, uint8_t intensity) {
    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs = (uint32_t)calcHold(80, 500, speed);
    const int      W      = (int)_config.getColCount();
    const int      H      = (int)_config.getRowCount();

    const uint8_t OPEN_SPEED  = 80;
    const uint8_t CLOSE_SPEED = OPEN_SPEED / 2;

    static constexpr int FACE_W = 9;
    static constexpr int FACE_H = 7;

    DEBUG_PRINTF("Generating smiley for %d cols and %d rows. Speed %d, intensity %d, target %d, step %u ms\n", W, H, speed, intensity, target, stepMs);

    SequenceEngine::Sequence seq = { .name = "smiley", .description = "Smiley", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false };

    if (W < FACE_W || H < FACE_H) {
        DEBUG_PRINTF("Smiley: display %dx%d too small (need %dx%d)\n", W, H, FACE_W, FACE_H);
        return seq;
    }

    const int startX = (W - FACE_W) / 2 + 1;   // centre horizontally
    const int startY = (H - FACE_H) / 2 + 1;   // centre vertically (1 on a 7-row display)

    // Static face: round outline + two eyes. The interior mouth rows are left
    // blank here because the mouth is drawn and animated separately.
    static const uint8_t FACE[FACE_H][FACE_W] = {
        {0,0,1,1,1,1,1,0,0},
        {0,1,0,0,0,0,0,1,0},
        {1,0,1,0,0,0,1,0,1},   // eyes at local cols 2 and 6
        {1,0,0,0,0,0,0,0,1},
        {1,0,0,0,0,0,0,0,1},
        {0,1,0,0,0,0,0,1,0},
        {0,0,1,1,1,1,1,0,0},
    };

    seq.playlist.reserve(FACE_W * FACE_H + 64);

    // Draw the outline + eyes (chained at zero hold, so the face pops in on one tick)
    for (int lr = 0; lr < FACE_H; ++lr)
        for (int lc = 0; lc < FACE_W; ++lc)
            if (FACE[lr][lc])
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                    startX + lc, startY + lr, 1, OPEN_SPEED, target, 0));

    // Mouth: 5 pins, one per column, whose row animates. Larger local row =
    // lower on the display. Straight baseline = 4; the smile lifts the corners
    // to 3 and drops the middle to 5.
    const int mouthCol[5] = {2, 3, 4, 5, 6};
    int curRow[5]         = {4, 4, 4, 4, 4};

    // Move the mouth pins to the given rows, closing each pin it leaves and
    // opening the new one, then hold. 'initial' draws the first mouth outright.
    auto drawMouth = [&](const int rows[5], uint32_t hold, bool initial) {
        const size_t frameStart = seq.playlist.size();
        for (int i = 0; i < 5; ++i) {
            if (!initial && rows[i] == curRow[i]) continue;
            if (!initial)
                seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                    startX + mouthCol[i], startY + curRow[i], 1, CLOSE_SPEED, 0, 0));
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                startX + mouthCol[i], startY + rows[i], 1, OPEN_SPEED, target, 0));
            curRow[i] = rows[i];
        }
        if (seq.playlist.size() > frameStart)
            seq.playlist.back().holdTimeMs = hold;
    };

    const int straight[5] = {4, 4, 4, 4, 4};
    const int corners[5]  = {3, 4, 4, 4, 3};
    const int smile[5]    = {3, 4, 5, 4, 3};

    drawMouth(straight, stepMs * 3, true);    // pose: face with a straight mouth
    drawMouth(corners,  stepMs,     false);   // corners curl up
    drawMouth(smile,    stepMs * 8, false);   // middle drops — hold the grin
    drawMouth(corners,  stepMs,     false);   // relax back...
    drawMouth(straight, stepMs * 4, false);   // ...to straight, then loop

    DEBUG_PRINTF("Smiley: %d cues generated\n", seq.playlist.size());
    return seq;
}

// ---------------------------------------------------------------------------
// Snake game sequence
// ---------------------------------------------------------------------------
// Pre-simulates a full self-playing snake game on the motor grid and encodes
// each move as delta MotionCues (extend new head, retract old tail).
// Food pixels appear as isolated extended pins; the snake body is a
// contiguous chain of extended pins navigating toward each food pixel via BFS.
// The snake grows from 1 to X pins long as it eats each food pixel.

SequenceEngine::Sequence SequenceGenerator::snake(uint8_t speed, uint8_t intensity) {
    const int32_t  target  = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs  = (uint32_t)calcHold(100, 600, speed);
    const int      W       = (int)_config.getColCount();
    const int      H       = (int)_config.getRowCount();

    struct SnakePos { int x, y; };

    // Flat index: x in [1..W], y in [1..H]
    auto idx = [&](int x, int y) -> int { return (x - 1) * H + (y - 1); };

    // BFS from head to goal; body[0]=head, body.back()=tail.
    // All body segments except the tail are treated as obstacles (tail will move).
    // Returns the sequence of positions to move through (not including head itself).
    auto findPath = [&](SnakePos head, SnakePos goal,
                        const std::deque<SnakePos>& body) -> std::vector<SnakePos> {
        std::vector<bool> blocked(W * H, false);
        for (size_t i = 0; i + 1 < body.size(); ++i)
            blocked[idx(body[i].x, body[i].y)] = true;

        std::vector<int> parent(W * H, -1);
        const int si = idx(head.x, head.y);
        parent[si] = si;  // self-ref = visited marker for start

        std::queue<SnakePos> q;
        q.push(head);

        const int dx[] = { 1, -1, 0,  0 };
        const int dy[] = { 0,  0, 1, -1 };
        bool found = false;

        while (!q.empty()) {
            SnakePos c = q.front(); q.pop();
            if (c.x == goal.x && c.y == goal.y) { found = true; break; }
            for (int d = 0; d < 4; ++d) {
                int nx = c.x + dx[d], ny = c.y + dy[d];
                if (nx < 1 || nx > W || ny < 1 || ny > H) continue;
                int ni = idx(nx, ny);
                if (blocked[ni] || parent[ni] != -1) continue;
                parent[ni] = idx(c.x, c.y);
                q.push({nx, ny});
            }
        }

        if (!found) return {};

        // Reconstruct path from goal back to the first step after head
        std::vector<SnakePos> path;
        int ci = idx(goal.x, goal.y);
        while (ci != si) {
            path.push_back({ ci / H + 1, ci % H + 1 });
            ci = parent[ci];
        }
        std::reverse(path.begin(), path.end());
        return path;
    };

    // Pick a random grid position not occupied by the snake body
    auto pickFood = [&](const std::deque<SnakePos>& body) -> SnakePos {
        std::vector<SnakePos> avail;
        avail.reserve((size_t)(W * H) - body.size());
        for (int x = 1; x <= W; ++x) {
            for (int y = 1; y <= H; ++y) {
                bool occ = false;
                for (const auto& p : body) {
                    if (p.x == x && p.y == y) { occ = true; break; }
                }
                if (!occ) avail.push_back({x, y});
            }
        }
        if (avail.empty()) return {-1, -1};
        return avail[esp_random() % avail.size()];
    };

    SequenceEngine::Sequence seq = {
        .name = "snake", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false
    };
    seq.playlist.reserve(512);

    std::deque<SnakePos> body;
    body.push_back({ (W + 1) / 2, (H + 1) / 2 });

    // Extend the initial snake pin, then extend the first food pin and hold
    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
        body.front().x, body.front().y, 1, 80, target, 0));

    SnakePos food = pickFood(body);
    if (food.x == -1) return seq;
    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
        food.x, food.y, 1, 80, target, stepMs));

    bool completed = false;
    const int maxSteps = W * H * 20;  // safety limit against infinite loops
    const int maxBodySize = 15;

    for (int step = 0; step < maxSteps && (int)body.size() < maxBodySize; ++step) {

        // Find the optimal path from head to food
        std::vector<SnakePos> path = findPath(body.front(), food, body);

        if (path.empty()) {
            // No path to food — take any valid adjacent step to keep moving
            const int dx[] = { 1, -1, 0,  0 };
            const int dy[] = { 0,  0, 1, -1 };
            SnakePos head = body.front();
            for (int d = 0; d < 4; ++d) {
                int nx = head.x + dx[d], ny = head.y + dy[d];
                if (nx < 1 || nx > W || ny < 1 || ny > H) continue;
                bool occ = false;
                for (size_t i = 0; i + 1 < body.size(); ++i) {
                    if (body[i].x == nx && body[i].y == ny) { occ = true; break; }
                }
                if (!occ) { path.push_back({nx, ny}); break; }
            }
            if (path.empty()) break;  // completely boxed in
        }

        SnakePos newHead = path[0];
        SnakePos oldTail = body.back();
        bool     ate     = (newHead.x == food.x && newHead.y == food.y);

        body.push_front(newHead);

        if (ate) {
            // Snake grew — tail stays, place new food
            if ((int)body.size() >= maxBodySize) { completed = true; break; }

            SnakePos newFood = pickFood(body);
            if (newFood.x == -1) break;

            // Extend new food pin; hold marks the end of this step
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                newFood.x, newFood.y, 1, 80, target, stepMs));
            food = newFood;
        } else {
            // Snake moved — retract the vacated tail pin
            body.pop_back();
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                newHead.x, newHead.y, 1, 80, target, 0));
            seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                oldTail.x, oldTail.y, 1, 80, 0, stepMs));
        }
    }

    // Final hold so the viewer can admire the finished (or stuck) snake
    if (!body.empty()) {
        const uint32_t finalHold = completed ? 5000u : 2000u;
        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
            body.front().x, body.front().y, 1, 80, target, finalHold));

        seq.playlist.emplace_back(_motorContol.createMoveMotionCue(0, 0, 1, 10, 0, 500)); // slowly reset display to empty

    }

    return seq;
}

// ---------------------------------------------------------------------------
// Static paged text display
// ---------------------------------------------------------------------------
// Renders the string one screen-width page at a time.  Each page is centred
// horizontally and holds for PAGE_HOLD_MS before transitioning to the next.
// Only motors whose state differs from the previous page receive a CAN command
// (delta rendering), keeping bus traffic low between pages.

SequenceEngine::Sequence SequenceGenerator::displayText(const char* text, uint8_t intensity) {

    const int32_t  target = calcTarget(0, _config.getMaxTarget(), intensity);
    const int      W      = static_cast<int>(_config.getColCount());
    const int      H      = static_cast<int>(_config.getRowCount());

    static constexpr int      FONT_H        = 5;
    static constexpr int      FONT_W        = 3;
    static constexpr int      CHAR_GAP      = 1;
    static constexpr uint32_t PAGE_HOLD_MS  = 2000u;

    // 1-indexed first row of the font band, centred vertically
    const int fontTopRow   = (H - FONT_H) / 2 + 1;
    // Number of chars that fit: n*(FONT_W+CHAR_GAP)-CHAR_GAP <= W  =>  n <= (W+CHAR_GAP)/(FONT_W+CHAR_GAP)
    const int charsPerPage = (W + CHAR_GAP) / (FONT_W + CHAR_GAP);

    SequenceEngine::Sequence seq = {
        .name = "displaytext", .description = "Display text", .type = 0, .playlist = {}, .speed = 100, .intensity = intensity, .loop = false, .data = {0}
    };
    strlcpy(seq.data, text, sizeof(seq.data) - 1);
    
    const int textLen = static_cast<int>(strlen(text));
    if (textLen == 0 || charsPerPage == 0) return seq;

    const int pageCount = (textLen + charsPerPage - 1) / charsPerPage;
    seq.playlist.reserve(static_cast<size_t>(pageCount) * W * H + 1);

    // Mirror of motor states so we can do delta updates between pages.
    // Index: (x-1)*H + (y-1).  0 = retracted, 1 = extended.
    std::vector<uint8_t> motorState(static_cast<size_t>(W) * H, 0u);

    for (int page = 0; page < pageCount; ++page) {
        esp_task_wdt_reset(); // need this to stop the WDT resetting the device as it takes a bit of processing

        const int pageStart = page * charsPerPage;
        const int pageLen   = std::min(charsPerPage, textLen - pageStart);

        // Pixel bitmap for this page
        const int textWidth = pageLen * (FONT_W + CHAR_GAP) - CHAR_GAP;
        const int startX    = (W - textWidth) / 2 + 1;  // 1-indexed, centred

        std::vector<uint8_t> desired(static_cast<size_t>(W) * H, 0u);

        for (int ci = 0; ci < pageLen; ++ci) {
            const bool (*bm)[3] = getScrollCharBitmap(text[pageStart + ci]);
            const int charX = startX + ci * (FONT_W + CHAR_GAP);
            for (int r = 0; r < FONT_H; ++r) {
                const int y = fontTopRow + r;
                if (y < 1 || y > H) continue;
                for (int fc = 0; fc < FONT_W; ++fc) {
                    const int x = charX + fc;
                    if (x < 1 || x > W) continue;
                    desired[(x - 1) * H + (y - 1)] = bm[r][fc] ? 1u : 0u;
                }
            }
        }

        // Emit delta cues (left-to-right, top-to-bottom raster order)
        const size_t frameStart = seq.playlist.size();
        for (int x = 1; x <= W; ++x) {
            for (int y = 1; y <= H; ++y) {
                const size_t i = static_cast<size_t>(x - 1) * H + (y - 1);
                if (desired[i] != motorState[i]) {
                    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                        x, y, 1, 80, desired[i] ? target : 0, 0));
                    motorState[i] = desired[i];
                }
            }
        }

        // Attach the page hold to the last cue; if nothing changed emit a no-op
        if (seq.playlist.size() > frameStart) {
            seq.playlist.back().holdTimeMs = PAGE_HOLD_MS;
        } else {
            seq.playlist.emplace_back(
                _motorContol.createMoveMotionCue(1, 1, 1, 80, 0, PAGE_HOLD_MS));
        }
    }

    return seq;
}

// ---------------------------------------------------------------------------
// Scrolling text sequence
// ---------------------------------------------------------------------------
// Renders a string into a virtual canvas using the 3×5 font defined above,
// then scrolls it right-to-left across the motor grid using delta rendering:
// only motors whose state changes between consecutive 1-pixel scroll positions
// receive a CAN command (typically ≤7 per step — one column of rows).
//
// Virtual canvas layout:
//   [W blank cols] [rendered text] [W blank cols]
// The text enters from the right edge and fully exits off the left edge.
// Font rows occupy display rows fontTopRow … fontTopRow+4 (centred vertically).

SequenceEngine::Sequence SequenceGenerator::scrollText(const char* text, uint8_t speed, uint8_t intensity) {

    const int32_t  target  = calcTarget(0, _config.getMaxTarget(), intensity);
    const uint32_t stepMs  = static_cast<uint32_t>(calcHold(20, 500, speed));
    const int      W       = static_cast<int>(_config.getColCount());
    const int      H       = static_cast<int>(_config.getRowCount());

    static constexpr int FONT_H   = 5;
    static constexpr int FONT_W   = 3;
    static constexpr int CHAR_GAP = 1;

    // First display row (1-indexed) at which the font is drawn
    const int fontTopRow = (H - FONT_H) / 2 + 1;   // 2 for a 7-row display

    // ---- Build virtual canvas ----------------------------------------
    const int textLen   = static_cast<int>(strlen(text));
    const int textWidth = textLen > 0 ? textLen * (FONT_W + CHAR_GAP) - CHAR_GAP : 0;
    const int canvasW   = W + textWidth + W;  // lead-blank + text + trail-blank

    // Flat canvas: canvas[col * FONT_H + fontRow]  (single allocation, no vector<bool>)
    std::vector<uint8_t> canvas(static_cast<size_t>(canvasW) * FONT_H, 0u);

    int cx = W;   // canvas column of first character's left edge
    for (int i = 0; i < textLen; ++i) {
        const bool (*bm)[3] = getScrollCharBitmap(text[i]);
        for (int r = 0; r < FONT_H; ++r)
            for (int c = 0; c < FONT_W; ++c)
                canvas[(cx + c) * FONT_H + r] = bm[r][c] ? 1u : 0u;
        cx += FONT_W;
        if (i < textLen - 1) cx += CHAR_GAP;
    }

    // ---- Sequence setup -----------------------------------------------
    SequenceEngine::Sequence seq = {
        .name = "scrolltext", .type = 0, .playlist = {}, .speed = speed, .intensity = intensity, .loop = false, .data = {0}
    };
    strlcpy(seq.data, text, sizeof(seq.data) - 1);
    seq.playlist.reserve(static_cast<size_t>(canvasW) * 10 + 1);

    // Flat display-state mirror: display[(x-1) * H + (y-1)]  (all motors start retracted)
    std::vector<uint8_t> display(static_cast<size_t>(W) * H, 0u);

    // ---- Scroll one pixel per step ------------------------------------
    for (int s = 1; s <= canvasW - W; ++s) {

        // Reset the task watchdog directly on behalf of the currently-running
        // task (async_tcp).  yield() is not sufficient here because it switches
        // to other tasks but returns to this callback — never to the async_tcp
        // event-loop code that would normally call esp_task_wdt_reset().
        esp_task_wdt_reset();

        const size_t frameStart = seq.playlist.size();

        for (int x = 1; x <= W; ++x) {
            const int canvasCol = s + x - 1;
            for (int y = 1; y <= H; ++y) {
                const int    fontRow = y - fontTopRow;   // 0-indexed within font
                const uint8_t desired = (fontRow >= 0 && fontRow < FONT_H)
                                        ? canvas[canvasCol * FONT_H + fontRow]
                                        : 0u;
                uint8_t& cur = display[(x - 1) * H + (y - 1)];
                if (desired != cur) {
                    seq.playlist.emplace_back(_motorContol.createMoveMotionCue(
                        x, y, 1, 80, desired ? target : 0, 0));
                    cur = desired;
                }
            }
        }

        // Attach the per-step hold to the last cue of this frame.
        // If nothing changed (blank region), emit a no-op hold on motor (1,1).
        if (seq.playlist.size() > frameStart) {
            seq.playlist.back().holdTimeMs = stepMs;
        } else {
            seq.playlist.emplace_back(
                _motorContol.createMoveMotionCue(1, 1, 1, 80, 0, stepMs));
        }
    }

    return seq;
}
