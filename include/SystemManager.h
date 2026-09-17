#pragma once
#ifndef _SYSTEMMGR_h
#define _SYSTEMMGR_h

#include "Config.h"
#include "CANBus.h"

class SystemManager{

    public:
        struct Status{
            bool wiFiConnected;
            String wifiIp;
            int32_t wifiChannel;
            int32_t wifiQuality;
            int32_t wifiRssi;
            String wifiGatewayIP;
            String wifiMACAddress;
            const char * wifiMDNSName;
            size_t memoryFreeHeap;
            size_t memoryMaxBlock;
            uint32_t memoryMinFreeHeap;
            uint32_t psramSize;
            uint32_t psramFree;
            bool CANStarted;
            uint16_t CANSpeedKbps;
            uint32_t CANSentCounter;
            uint32_t CANReceivedCounter;
            uint32_t CANFailedCounter;
            uint32_t cpuFreqMHz;
            const char * chipModel;
            uint32_t uptimeSeconds;

        };
        SystemManager(Config& config, CANBus& can);
        Status getStatus();
        void scanModules();
        void resetModule(uint32_t moduleId);

    private:
        Config& _config;
        CANBus& _can;
};

#endif