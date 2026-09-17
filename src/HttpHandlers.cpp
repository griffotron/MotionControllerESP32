
#include <ESPAsyncWebServer.h>
#include <AsyncJson.h>
#if __has_include("ArduinoJson.h")
  #include <ArduinoJson.h>
  #include <AsyncJson.h>
#endif
#include <LittleFS.h>

#include "globals.h"
#include "HttpHandlers.h"
extern bool clockMode;   // defined in main.cpp, tracks whether the clock is the active sequence
// Demo mode state, defined in main.cpp. The loop there runs the actual cycle;
// these just let the HTTP layer start it and tune the interval.
extern bool demoMode;
extern uint32_t demoIntervalMs;
extern bool demoRestart;
#include "MotorControl.h"
#include "SequenceEngine.h"
#include "SystemManager.h"
#include "DataStructures.h"
#include "Constants.h"   // SEQUENCE_DIR
#include <ui/index_gz.h>
#include <ui/vite_gz.h>


#define DEBUG true  //set to true for debug output, false for no debug output
#define DEBUG_PRINT if(DEBUG)Serial
#define DEBUG_PRINTF if(DEBUG)printf

HttpHandlers::HttpHandlers(Config& config, AsyncWebServer& server, MotorControl& motorControl, SequenceEngine& sequenceEngine, SequenceGenerator& sequenceGenerator, SystemManager& systemMgr) : 
_config(config), _server(server), _motorControl(motorControl), _sequenceEngine(sequenceEngine), _sequenceGenerator(sequenceGenerator), _systemMgr(systemMgr)
{
}

void HttpHandlers::setup() {
    addHandlerRoot();
    addHandlerApiStatus();
    addHandlerApiMoveToTarget();
    addHandlerApiHome();
    addHandlerApiZero();
    addHandlerApiModuleReset();
    addHandlerApiSequenceLoad();
    addHandlerApiSequenceGenerate();
    addHandlerApiSequenceControl();
    addHandlerApiSequenceGenerators();
    addHandlerApiSequenceAsJson();
    addHandlerApiSaveSequence();
    addHandlerApiDeleteSequence();
    addHandlerApiSavedSequences();
    addHandlerApiDemo();
    addHandlerApiConfig();
    addHandlerApiFetchModules();
    
    _server.onNotFound([](AsyncWebServerRequest *request){
        if(request->method() == HTTP_OPTIONS){
        request->send(200);
        }
        else{
        Serial.print("Not found");
        request->send(404, "Not found");
        }
  });
}

void HttpHandlers::addHandlerRoot(){
    _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("Root URL requested");
        AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_gz, index_gz_len);
        response->addHeader("Content-Encoding", "gzip");
        request->send(response);
    });
}

void HttpHandlers::addHandlerApiStatus(){
    _server.on("/api/system/status", HTTP_GET, [&](AsyncWebServerRequest *request){
        SystemManager::Status status = _systemMgr.getStatus();
        JsonDocument doc;
        doc["canStarted"] = status.CANStarted;
        doc["canSpeedKbps"] = status.CANSpeedKbps;
        doc["canSentCounter"] = status.CANSentCounter;
        doc["canReceivedCounter"] = status.CANReceivedCounter;
        doc["canFailedCounter"] = status.CANFailedCounter;
        doc["cpuFreqMHz"] = status.cpuFreqMHz;
        doc["chipModel"] = status.chipModel;
        doc["memoryFreeHeap"] = status.memoryFreeHeap;
        doc["memoryMaxBlock"] = status.memoryMaxBlock;
        doc["memoryMinFreeHeap"] = status.memoryMinFreeHeap;
        doc["psramFree"] = status.psramFree;
        doc["psramSize"] = status.psramSize;
        doc["wifiChannel"] = status.wifiChannel;
        doc["wiFiConnected"] = status.wiFiConnected;
        doc["wifiGatewayIP"] = status.wifiGatewayIP;
        doc["wifiIp"] = status.wifiIp;
        doc["wifiMACAddress"] = status.wifiMACAddress;
        doc["wifiMDNSName"] = status.wifiMDNSName;
        doc["wifiQuality"] = status.wifiQuality;
        doc["wifiRssi"] = status.wifiRssi;
        doc["uptimeSeconds"] = status.uptimeSeconds;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
}

void HttpHandlers::addHandlerApiFetchModules(){
    _server.on("/api/system/modules", HTTP_GET, [&](AsyncWebServerRequest *request){
        DEBUG_PRINTF("Request to %s\n", request->url());
        // Returns a list of module Ids that the Pixel Map is expecting to exist

        JsonDocument doc;
        JsonArray moduleIds = doc.to<JsonArray>();
        
        for(uint32_t moduleId : _config.getUniqueModuleIds()){
            moduleIds.add(moduleId);
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
}

void HttpHandlers::addHandlerApiMoveToTarget(){
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/motion/move", [&](AsyncWebServerRequest *request, JsonVariant &json){
        DEBUG_PRINTF("Request to %s\n", request->url());

        const JsonObject &jsonObj = json.as<JsonObject>();
        int32_t target = jsonObj["target"].as<int32_t>(); 
        JsonArray pixelArray = jsonObj["pixels"].as<JsonArray>(); 
        uint32_t priority = jsonObj["priority"].as<uint32_t>(); 
        uint32_t speed = jsonObj["speed"].as<uint32_t>(); 

        DEBUG_PRINT.print("New target: ");
        DEBUG_PRINT.println(target);

        std::vector<std::array<uint32_t, 2>> pixelList;
        pixelList.reserve(pixelArray.size());

        for (JsonArray pixelXY : pixelArray) {
            pixelList.push_back({pixelXY[0], pixelXY[1]});
        }

        _motorControl.move(pixelList, priority, speed, target);
        request->send(200, "application/json", "{\"result\":\"success\"}");
  }));
}


void HttpHandlers::addHandlerApiHome(){
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/motion/home", [&](AsyncWebServerRequest *request, JsonVariant &json){
        DEBUG_PRINTF("Request to %s\n", request->url());

        const JsonObject &jsonObj = json.as<JsonObject>();
        JsonArray pixelArray = jsonObj["pixels"].as<JsonArray>(); 
        DEBUG_PRINT.println("Homing request");

        std::vector<std::array<uint32_t, 2>> pixelList;
        pixelList.reserve(pixelArray.size());

        for (JsonArray pixelXY : pixelArray) {
            pixelList.push_back({pixelXY[0], pixelXY[1]});
        }

        if(pixelList.size() == 0 || (pixelList[0][0] == 0 && pixelList[0][1] == 0)){
            _sequenceEngine.load(_sequenceGenerator.homeAll());
        }
        else{
            _sequenceEngine.load(_sequenceGenerator.home(pixelList));
        }
        request->send(200, "application/json", "{\"result\":\"success\"}");
  }));
}

void HttpHandlers::addHandlerApiZero(){
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/motion/zero", [&](AsyncWebServerRequest *request, JsonVariant &json){
        DEBUG_PRINTF("Request to %s\n", request->url());

        const JsonObject &jsonObj = json.as<JsonObject>();
        JsonArray pixelArray = jsonObj["pixels"].as<JsonArray>(); 
        DEBUG_PRINT.println("Set current position to zero request");

        std::vector<std::array<uint32_t, 2>> pixelList;
        pixelList.reserve(pixelArray.size());

        for (JsonArray pixelXY : pixelArray) {
            _sequenceEngine.inject(_motorControl.createResetMotorPositionMotionCue(pixelXY[0], pixelXY[1], 1, 0));
        }

        request->send(200, "application/json", "{\"result\":\"success\"}");
  }));
}

void HttpHandlers::addHandlerApiModuleReset(){
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/module/reset", [&](AsyncWebServerRequest *request, JsonVariant &json){
        DEBUG_PRINTF("Request to %s\n", request->url());

        const JsonObject &jsonObj = json.as<JsonObject>();
        uint32_t moduleId = jsonObj["moduleId"].as<uint32_t>(); 
        DEBUG_PRINTF("Reset module %u\n", moduleId);

        _systemMgr.resetModule(moduleId);

        request->send(200, "application/json", "{\"result\":\"success\"}");
  }));
}

void HttpHandlers::addHandlerApiSaveSequence(){
    _server.on("/api/sequence/save", HTTP_POST, [&](AsyncWebServerRequest *request) {
        DEBUG_PRINTF("Request to %s\n", request->url());

        SequenceEngine::Sequence seq = _sequenceEngine.getCurrentSequence();

        // Filename is the sequence name with speed and intensity appended,
        // hyphen-separated, e.g. "plasma-50-100" -> /sequences/plasma-50-100.json
        String fileName = String(seq.name) + "-" + String(seq.speed) + "-" + String(seq.intensity);

        bool ok = _sequenceGenerator.saveToFile(seq, fileName.c_str());

        JsonDocument doc;
        doc["result"] = ok ? "success" : "error";
        doc["name"] = fileName;
        doc["description"] = seq.description;
        String response;
        serializeJson(doc, response);
        request->send(ok ? 200 : 500, "application/json", response);

    });

}

void HttpHandlers::addHandlerApiDeleteSequence(){
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/sequence/delete", [&](AsyncWebServerRequest *request, JsonVariant &json){
        DEBUG_PRINTF("Request to %s\n", request->url());
        const JsonObject &jsonObj = json.as<JsonObject>();
        String name = jsonObj["fileName"].as<String>();

        // Reject anything that could escape the /sequences/ folder
        if (name.length() == 0 || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 || name.indexOf("..") >= 0) {
            request->send(400, "application/json", "{\"result\":\"error\",\"details\":\"Invalid name\"}");
            return;
        }

        char filePath[80];
        snprintf(filePath, sizeof(filePath), "%s/%s.json", SEQUENCE_DIR, name.c_str());

        if (!LittleFS.exists(filePath)) {
            request->send(404, "application/json", "{\"result\":\"error\",\"details\":\"Not found\"}");
            return;
        }

        bool ok = LittleFS.remove(filePath);
        DEBUG_PRINTF("Delete '%s' -> %s\n", filePath, ok ? "ok" : "failed");
        request->send(ok ? 200 : 500, "application/json",
            ok ? "{\"result\":\"success\"}" : "{\"result\":\"error\"}");
    }));
}

void HttpHandlers::addHandlerApiSavedSequences(){
    _server.on("/api/sequence/saved", HTTP_GET, [&](AsyncWebServerRequest *request){
        DEBUG_PRINTF("Request to %s\n", request->url());

        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();

        // Filter so we only read the header fields from each file and skip the
        // big "sequence" array, keeps this fast and light regardless of size.
        JsonDocument filter;
        filter["name"] = true;
        filter["description"] = true;
        filter["speed"] = true;
        filter["intensity"] = true;
        filter["data"] = true;

        File dir = LittleFS.open(SEQUENCE_DIR);
        if (dir && dir.isDirectory()) {
            File f = dir.openNextFile();
            while (f) {
                if (!f.isDirectory()) {
                    String base = f.name();
                    int slash = base.lastIndexOf('/');   // name() may include the path
                    if (slash >= 0) base = base.substring(slash + 1);

                    if (base.endsWith(".json")) {
                        JsonDocument meta;
                        DeserializationError err = deserializeJson(meta, f, DeserializationOption::Filter(filter));
                        if (err) {
                            DEBUG_PRINTF("Skipping cached '%s': %s\n", base.c_str(), err.f_str());
                        } else {
                            // The filename (minus .json) is the key used to load it
                            String key = base.substring(0, base.length() - 5);
                            String desc = meta["description"].as<String>();
                            if (desc.isEmpty()) desc = key;

                            JsonObject obj = array.add<JsonObject>();
                            obj["name"] = meta["name"].as<String>();
                            obj["fileName"] = key;
                            obj["description"] = desc;
                            obj["type"] = 1;                             // file-loaded
                            obj["speed"] = meta["speed"].as<uint8_t>();
                            obj["intensity"] = meta["intensity"].as<uint8_t>();
                            obj["data"] = meta["data"].as<String>();
                            obj["intensityControl"] = false;            // pre-rendered: controls don't apply
                            obj["speedControl"] = false;
                            obj["dataControl"] = false;
                        }
                    }
                }
                f = dir.openNextFile();
            }
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
}

void HttpHandlers::addHandlerApiDemo(){
    // Starts demo mode: the main loop shows a random saved sequence, then the
    // clock for 'minutes', then another random saved sequence, and so on.
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/sequence/demo", [&](AsyncWebServerRequest *request, JsonVariant &json){
        DEBUG_PRINTF("Request to %s\n", request->url());
        const JsonObject &jsonObj = json.as<JsonObject>();

        uint32_t minutes = jsonObj["minutes"].as<uint32_t>();
        if (minutes < 1) minutes = 5;   // floor/default so the interval is always sane

        demoIntervalMs = minutes * 60u * 1000u;
        clockMode = false;   // demo owns the display; its own loop keeps the clock ticking
        demoRestart = true;  // signals the main loop to (re)start the cycle from a fresh sequence
        demoMode = true;

        DEBUG_PRINTF("Demo mode started, interval %u min\n", minutes);
        request->send(200, "application/json", "{\"result\":\"success\"}");
    }));
}

void HttpHandlers::addHandlerApiSequenceLoad(){
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/sequence/load", [&](AsyncWebServerRequest *request, JsonVariant &json){
        DEBUG_PRINTF("Request to %s\n", request->url());
        const JsonObject &jsonObj = json.as<JsonObject>();
        String sequence = jsonObj["fileName"].as<String>();
        DEBUG_PRINTF("Load sequence '%s'\n", sequence.c_str());
        clockMode = false;  // cleared here; re-set below if the sequence is "clock"
        demoMode = false;   // any manual load stops demo mode
        // Pause the outgoing sequence before clearing so its timer can't re-open
        // pins while the next sequence is being generated. load() resumes the
        // engine when the new sequence takes over.
        _sequenceEngine.pause();
        _sequenceEngine.inject(_motorControl.createMoveMotionCue(0, 0, 1, 80, 0, 0));
        
        DEBUG_PRINTF("Load stored sequence named: %s\n", sequence.c_str());

        if(sequence == "clock"){ // special case for clock
            clockMode = true;
            _sequenceEngine.load(_sequenceGenerator.displayCurrentTime());
        }
        else{
            char filePath[64];
            snprintf(filePath, sizeof(filePath), "%s/%s.json", SEQUENCE_DIR, sequence.c_str());

            _sequenceEngine.load(_sequenceGenerator.loadFromFile(filePath)); // filepath validation done in sequence generator
        }

        request->send(200, "application/json", "{\"result\":\"success\"}");
  }));
}

void HttpHandlers::addHandlerApiSequenceAsJson(){
    _server.on("/api/sequence/json", HTTP_GET, [&](AsyncWebServerRequest *request) {
        DEBUG_PRINTF("Request to %s\n", request->url());
        JsonDocument doc = _sequenceGenerator.toJson(_sequenceEngine.getCurrentSequence());

        if(doc.overflowed()){
            request->send(500, "application/json", "{\"result\":\"error\",\"details\":\"Document too large\"}");
            return;
        }

        AsyncResponseStream *response = request->beginResponseStream("application/json");
        serializeJson(doc, *response);
        request->send(response);

    });

}

void HttpHandlers::addHandlerApiSequenceGenerators(){
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/sequence/generators", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        DEBUG_PRINTF("Request to %s\n", request->url());
        std::vector<Sequence> sequences;
        
        //key, name, type, speed, intensity, data
        sequences.push_back(Sequence("flash", "Flash", 0, 15, 100, "", true, true, false));
        sequences.push_back(Sequence("hwave", "Horizontal Wave", 0, 80, 100, "", true, true, false));
        sequences.push_back(Sequence("vwave", "Vertical Wave", 0, 30, 100, "", true, true, false));
        sequences.push_back(Sequence("hsway", "Sway", 0, 30, 100, "", false, true, false));
        sequences.push_back(Sequence("bounce", "Bounce", 0, 50, 100, "", true, true, false));
        sequences.push_back(Sequence("matrix", "Matrix", 0, 50, 100, "", true, true, false));
        sequences.push_back(Sequence("starfield", "Star Field", 0, 20, 100, "", true, true, false));
        sequences.push_back(Sequence("ripples", "Ripples", 0, 50, 100, "", true, true, false));
        sequences.push_back(Sequence("fire", "Fire", 0, 70, 100, "", true, true, false));
        sequences.push_back(Sequence("equalizer", "Equalizer", 0, 60, 100, "", true, true, false));
        sequences.push_back(Sequence("fireworks", "Fireworks", 0, 50, 100, "", true, true, false));
        sequences.push_back(Sequence("snowfall", "Snowfall", 0, 40, 100, "", true, true, false));
        sequences.push_back(Sequence("oscilloscope", "Oscilloscope", 0, 60, 100, "", true, true, false));
        sequences.push_back(Sequence("life", "Game of Life", 0, 50, 100, "", true, true, false));
        sequences.push_back(Sequence("plasma", "Plasma", 0, 50, 100, "", true, true, false));
        sequences.push_back(Sequence("flow", "Flow Field", 0, 40, 100, "", true, true, false));
        sequences.push_back(Sequence("radar", "Radar", 0, 60, 100, "", true, true, false));
        sequences.push_back(Sequence("smiley", "Smiley", 0, 50, 100, "", true, true, false));
        sequences.push_back(Sequence("chase", "Chase", 0, 70, 100, "", true, true, false));
        sequences.push_back(Sequence("spiral", "Spiral", 0, 100, 100, "", true, true, false));
        sequences.push_back(Sequence("burst", "Burst", 0, 40, 100, "", true, true, false));
        sequences.push_back(Sequence("countdown", "Countdown", 1, 70, 100, "", false, false, false));
        sequences.push_back(Sequence("clock", "Clock (24h)", 0, 100, 100, "", false, false, false));
        sequences.push_back(Sequence("snake", "Snake", 0, 50, 100, "", true, true, false));
        sequences.push_back(Sequence("scrolltext", "Scroll Text", 0, 50, 100, "", true, true, true));
        sequences.push_back(Sequence("displaytext", "Display Text", 0, 50, 100, "", true, true, true));

        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();

        for (const auto& seq : sequences) {
            JsonObject obj = array.add<JsonObject>();
            obj["name"] = seq.name;
            obj["description"] = seq.description;
            obj["type"] = seq.type;
            obj["speed"] = seq.speed;
            obj["intensity"] = seq.intensity;
            obj["data"] = seq.data;
            obj["intensityControl"] = seq.intensityControl;
            obj["speedControl"] = seq.speedControl;
            obj["dataControl"] = seq.dataControl;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    }));
}


void HttpHandlers::addHandlerApiSequenceGenerate(){
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/sequence/generate", [&](AsyncWebServerRequest *request, JsonVariant &json){
        DEBUG_PRINTF("Request to %s\n", request->url());
        const JsonObject &jsonObj = json.as<JsonObject>();
        //const char* sequence = jsonObj["name"].as<const char*>();
        String sequence = jsonObj["name"].as<String>();
        uint8_t type = jsonObj["type"].as<uint8_t>(); 
        uint8_t speed = jsonObj["speed"].as<uint8_t>(); 
        uint8_t intensity = jsonObj["intensity"].as<uint8_t>(); 
        String text = jsonObj["data"].as<String>();
        if(text.length() == 0){
            text = "";
        } 
        DEBUG_PRINTF("Load sequence '%s', type: %d, data: %s\n", sequence.c_str(), type, text.c_str());
        clockMode = false;  // cleared here; re-set below if the sequence is "clock"
        demoMode = false;   // any manual generate stops demo mode
        // Pause the outgoing sequence before clearing so its timer can't re-open
        // pins while the next sequence is being generated. load() resumes the
        // engine when the new sequence takes over.
        _sequenceEngine.pause();
        _sequenceEngine.inject(_motorControl.createMoveMotionCue(0, 0, 1, 80, 0, 0));

        if(sequence == "vwave"){
            _sequenceEngine.load(_sequenceGenerator.verticalWave(speed, intensity));
        }
        else if(sequence == "hwave"){
            _sequenceEngine.load(_sequenceGenerator.horizontalWave(speed, intensity));
        }
        else if(sequence == "flash"){
            _sequenceEngine.load(_sequenceGenerator.flash(speed, intensity));
        }
        else if(sequence == "hsway"){
            _sequenceEngine.load(_sequenceGenerator.horizontalSway(speed, intensity));
        }
        else if(sequence == "bounce") {
            _sequenceEngine.load(_sequenceGenerator.bouncingBall(speed, intensity));
        }
        else if(sequence == "matrix") {
            _sequenceEngine.load(_sequenceGenerator.matrix(speed, intensity));
        }
        else if(sequence == "starfield") {
            _sequenceEngine.load(_sequenceGenerator.starfield(speed, intensity));
        }
        else if(sequence == "ripples") {
            _sequenceEngine.load(_sequenceGenerator.ripples(speed, intensity));
        }
        else if(sequence == "fire") {
            _sequenceEngine.load(_sequenceGenerator.fire(speed, intensity));
        }
        else if(sequence == "equalizer") {
            _sequenceEngine.load(_sequenceGenerator.equalizer(speed, intensity));
        }
        else if(sequence == "fireworks") {
            _sequenceEngine.load(_sequenceGenerator.fireworks(speed, intensity));
        }
        else if(sequence == "snowfall") {
            _sequenceEngine.load(_sequenceGenerator.snowfall(speed, intensity));
        }
        else if(sequence == "oscilloscope") {
            _sequenceEngine.load(_sequenceGenerator.oscilloscope(speed, intensity));
        }
        else if(sequence == "life") {
            _sequenceEngine.load(_sequenceGenerator.gameOfLife(speed, intensity));
        }
        else if(sequence == "plasma") {
            _sequenceEngine.load(_sequenceGenerator.plasma(speed, intensity));
        }
        else if(sequence == "flow") {
            _sequenceEngine.load(_sequenceGenerator.flowField(speed, intensity));
        }
        else if(sequence == "radar") {
            _sequenceEngine.load(_sequenceGenerator.radar(speed, intensity));
        }
        else if(sequence == "smiley") {
            _sequenceEngine.load(_sequenceGenerator.smiley(speed, intensity));
        }
        else if(sequence == "chase"){
            _sequenceEngine.load(_sequenceGenerator.chase(speed, intensity));
        }
        else if(sequence == "spiral"){
            _sequenceEngine.load(_sequenceGenerator.spiral(speed, intensity));
        }
        else if(sequence == "burst"){
            _sequenceEngine.load(_sequenceGenerator.burst(speed, intensity));
        }
        else if(sequence == "clock"){
            // speed/intensity sliders are not used for the clock, time governs everything.
            clockMode = true;
            _sequenceEngine.load(_sequenceGenerator.displayCurrentTime());
        }
        else if(sequence == "snake"){
            _sequenceEngine.load(_sequenceGenerator.snake(speed, intensity));
        }
        else if(sequence == "displaytext"){
            _sequenceEngine.load(_sequenceGenerator.displayText(text.c_str(), intensity));
        }
        else if(sequence == "scrolltext"){
            _sequenceEngine.load(_sequenceGenerator.scrollText(text.c_str(), speed, intensity));
        }
        else if(sequence == "home-all"){
            _sequenceEngine.load(_sequenceGenerator.homeAll());
        }
        else if(sequence == "idle"){
            _sequenceEngine.load(_sequenceGenerator.idle());
        }

        request->send(200, "application/json", "{\"result\":\"success\"}");
  }));
}

void HttpHandlers::addHandlerApiSequenceControl(){
    _server.addHandler(new AsyncCallbackJsonWebHandler("/api/sequence/control", [&](AsyncWebServerRequest *request, JsonVariant &json){
        DEBUG_PRINTF("Request to %s\n", request->url());
        const JsonObject &jsonObj = json.as<JsonObject>();
        uint32_t command = jsonObj["command"].as<uint32_t>();

        switch(command){
            case 1:
                DEBUG_PRINT.println("PLAY");
                _sequenceEngine.play();
                break;
            case 2:
                DEBUG_PRINT.println("PAUSE");
                _sequenceEngine.pause();
                break;
            case 3:
                DEBUG_PRINT.println("RESUME");
                _sequenceEngine.resume();
                break;
            case 4:
                DEBUG_PRINT.println("RESET"); // start again from beginning of playlist
                _sequenceEngine.reset();
                break;
            case 5: //loop enable
                DEBUG_PRINT.println("LOOP ENABLE");
                _sequenceEngine.loop(true);
                break;
            case 6: //loop disable
                DEBUG_PRINT.println("LOOP DISABLE");
                _sequenceEngine.loop(false);
                break;
            default:
                request->send(400, "application/json", "{\"result\":\"failed\", \"message\":\"Control command value not recognised\"}");
                return;
        }
        request->send(200, "application/json", "{\"result\":\"success\"}");
    }));
}

void HttpHandlers::addHandlerApiConfig(){
    // Endpoint to receive and save JSON settings
    _server.on("/api/system/config", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"result\":\"success\"}");
    }, NULL, [&](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        DEBUG_PRINTF("Request to %s\n", request->url());
        _config.save(data, len, index, total);
    });

    _server.on("/api/system/config", HTTP_GET, [&](AsyncWebServerRequest *request) {
        DEBUG_PRINTF("Request to %s\n", request->url());
        if (LittleFS.exists("/config.json")) {
            // Send the file directly from the filesystem
            request->send(LittleFS, _config.getFilePath().data(), "application/json");
        } else {
            // Return an empty object or error if file doesn't exist
            request->send(404, "application/json", "{\"result\":\"error\",\"details\":\"No settings found\"}");
        }
    });

}


