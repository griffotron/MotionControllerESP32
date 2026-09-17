
#pragma once
#include <ESPAsyncWebServer.h>
#include "Config.h"
#include "MotorControl.h"
#include "SequenceEngine.h"
#include "SequenceGenerator.h"
#include "SystemManager.h"

#ifndef _HTTPHANDLERS_h
#define _HTTPHANDLERS_h

class HttpHandlers{
	private:
        void addHandlerRoot();
        void addHandlerApiStatus();
        void addHandlerApiMoveToTarget();
        void addHandlerApiHome();
        void addHandlerApiZero();
        void addHandlerApiModuleReset();
        void addHandlerApiSequenceLoad();
        void addHandlerApiSequenceGenerate();
        void addHandlerApiSequenceControl();
        void addHandlerApiSequenceGenerators();
        void addHandlerApiSequenceAsJson();
        void addHandlerApiSaveSequence();
        void addHandlerApiDeleteSequence();
        void addHandlerApiSavedSequences();
        void addHandlerApiDemo();
        void addHandlerApiConfig();
        void addHandlerApiFetchModules();       
        
        MotorControl& _motorControl;
        SequenceEngine& _sequenceEngine;
        SequenceGenerator& _sequenceGenerator;
        AsyncWebServer& _server;
        Config& _config;
        SystemManager& _systemMgr;

        struct Sequence {
            String name;
            String description;
            uint8_t type;
            uint8_t speed;
            uint8_t intensity;
            String data; // extra bit of data, e.g. text string for displaying text
            bool intensityControl; // these govern what controls are shown in the UI
            bool speedControl;
            bool dataControl;
        };

	public:
        HttpHandlers(Config& config, AsyncWebServer& server, MotorControl& motorControl, SequenceEngine& sequenceEngine, SequenceGenerator& sequenceGenerator, SystemManager& systemMgr);
        void setup();
};

#endif

