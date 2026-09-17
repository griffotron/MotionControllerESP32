#include <WiFi.h>
#include "SystemManager.h"
#include "DataStructures.h"

#define DEBUG true  //set to true for debug output, false for no debug output
#define DEBUG_PRINT if(DEBUG)Serial
#define DEBUG_PRINTF if(DEBUG)printf

SystemManager::SystemManager(Config& config, CANBus& can): _config(config), _can(can)
{

}

SystemManager::Status SystemManager::getStatus(){

    Status status;
    if(WiFi.status() == WL_CONNECTED){
        status.wiFiConnected = true;
        status.wifiRssi = WiFi.RSSI();
        status.wifiIp = WiFi.localIP().toString();
        status.wifiChannel = WiFi.channel();
        status.wifiGatewayIP = WiFi.gatewayIP().toString();
        status.wifiMACAddress = WiFi.macAddress();
        status.wifiMDNSName = MDNSName;

        status.wifiQuality = 0;
        if(status.wifiRssi <= -100) status.wifiQuality = 0;
        else if(status.wifiRssi >= -50) status.wifiQuality = 100;
        else status.wifiQuality = 2 * (status.wifiRssi + 100);
    }

    status.memoryFreeHeap = ESP.getFreeHeap();
    status.memoryMaxBlock = ESP.getMaxAllocHeap();
    status.memoryMinFreeHeap = ESP.getMinFreeHeap();
    status.psramSize = ESP.getPsramSize();
    status.psramFree = ESP.getFreePsram();
    
    status.CANStarted = _can.started();
    status.CANSpeedKbps = _can.speedKbps();
    status.CANSentCounter = _can.sentCounter();
    status.CANReceivedCounter = _can.receivedCounter();
    status.CANFailedCounter = _can.failedCounter();

    status.cpuFreqMHz = ESP.getCpuFreqMHz();
    status.chipModel = ESP.getChipModel();
    status.uptimeSeconds = (uint32_t)(esp_timer_get_time() / 1000000ULL);

    return status;

}

void SystemManager::scanModules(){
    uint8_t messageType = CANMessageType::ModuleStatusRequest;
    uint32_t controllerAddress = _config.getCANId();
    uint8_t controllerAddress0 = controllerAddress & 0xFF;
    uint8_t controllerAddress1 = (controllerAddress >> 8) & 0xFF;
    uint8_t controllerAddress2 = (controllerAddress >> 16) & 0xFF;
    uint8_t controllerAddress3 = (controllerAddress >> 24) & 0xFF;

    std::vector<uint32_t> modules = _config.getUniqueModuleIds();
    DEBUG_PRINTF("Scanning for %d module(s)...\n", modules.size());
    
    for(uint8_t index = 0; index < modules.size(); index++){
        DEBUG_PRINTF("Scanning for module id %d\n", modules[index]);
        uint8_t data[8] = { messageType, 1, 0, 0, controllerAddress0, controllerAddress1, controllerAddress2, controllerAddress3 };
        _can.write(modules[index], data, index * 200); //send the messages with 200ms between them
    }
}

void SystemManager::resetModule(uint32_t moduleId){
    DEBUG_PRINTF("Sending reset command to module %u\n", moduleId);
    uint8_t data[8] = { CANMessageType::FullReset, 1, 0, 0, 0, 0, 0, 0};
    _can.write(moduleId, data);
}