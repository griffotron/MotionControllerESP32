#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "WebSocketHandler.h"
#include "SystemManager.h"
#include "Config.h"
#include "CANBus.h"

#define DEBUG true  //set to true for debug output, false for no debug output
#define DEBUG_PRINT if(DEBUG)Serial
#define DEBUG_PRINTF if(DEBUG)printf

WebSocketHandler::WebSocketHandler(AsyncWebServer& server, AsyncWebSocket& webSocket, Config& config, SystemManager& systemMgr, SequenceEngine& sequenceEngine) :
 _server(server), _webSocket(webSocket), _config(config), _systemMgr(systemMgr), _sequenceEngine(sequenceEngine)
{

}

void WebSocketHandler::setup(){
    _webSocket.onEvent([this](AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type, void *a, uint8_t *data, size_t l) {
        if(type == WS_EVT_CONNECT){
            DEBUG_PRINTF("Client connected to web socket\n");
            // Send the current sequence status to the freshly-connected client so
            // its play/pause/loop buttons are correct without an HTTP round-trip.
            c->text(sequenceStatusJson(_sequenceEngine.getStatus()));
        }

        if(type == WS_EVT_DISCONNECT){
            DEBUG_PRINTF("Client disconnected from web socket\n");
        }

        if(type == WS_EVT_DATA) {
            DEBUG_PRINTF("Received data\n");
            JsonDocument doc;
            deserializeJson(doc, (const char*)data, l);
            
            if (doc["action"] == "scan-modules") {
                DEBUG_PRINTF("Web Socket command to scan modules\n");
                _systemMgr.scanModules();
            }
        }
    });
    _server.addHandler(&_webSocket);
}

void WebSocketHandler::broadcastModuleStatus(CanFrame& frame){
        uint8_t status = frame.data[2];
       	uint8_t moduleId0 = frame.data[4];
        uint8_t moduleId1 = frame.data[5];
        uint8_t moduleId2 = frame.data[6];
        uint8_t moduleId3 = frame.data[7];
        uint32_t moduleId = ((uint32_t)moduleId3 << 24) | ((uint32_t)moduleId2 << 16) | ((uint32_t)moduleId1 << 8) | (uint32_t)moduleId0;
    
        JsonDocument doc;
        JsonObject moduleObj;

        DEBUG_PRINTF("Received module status. Module: %d Status: %d\n", moduleId, status);

        moduleObj = doc["data"].to<JsonObject>();
        moduleObj["moduleId"] = moduleId;
        moduleObj["status"] = status;
        doc["type"] = "module-scan";

        String jsonString;
        serializeJson(doc, jsonString);
        _webSocket.textAll(jsonString);
}

String WebSocketHandler::sequenceStatusJson(const SequenceEngine::Status& status){
    JsonDocument doc;
    doc["type"] = "sequence-status";

    JsonObject data = doc["data"].to<JsonObject>();
    data["name"] = status.name;
    data["description"] = status.description;
    data["ready"] = status.ready;
    data["paused"] = status.paused;
    data["loop"] = status.loop;
    data["speed"] = status.speed;
    data["intensity"] = status.intensity;
    data["data"] = status.data;

    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

void WebSocketHandler::broadcastSequenceStatus(const SequenceEngine::Status& status){
    _webSocket.textAll(sequenceStatusJson(status));
}


