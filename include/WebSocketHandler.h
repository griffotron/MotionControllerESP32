#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "Config.h"
#include "SystemManager.h"
#include "MotorControl.h"
#include "SequenceEngine.h"

#ifndef _WSHANDLER_h
#define _WSHANDLER_h

class WebSocketHandler{

        public:
            WebSocketHandler(AsyncWebServer& server, AsyncWebSocket& webSocket, Config& config, SystemManager& systemMgr, SequenceEngine& sequenceEngine);
            void setup();
            void broadcastModuleStatus(CanFrame& frame);
            void broadcastSequenceStatus(const SequenceEngine::Status& status);

        private:
            AsyncWebSocket& _webSocket;
            AsyncWebServer& _server;
            Config& _config;
            SystemManager& _systemMgr;
            SequenceEngine& _sequenceEngine;

            String sequenceStatusJson(const SequenceEngine::Status& status);
            void scanModules();
};


#endif
