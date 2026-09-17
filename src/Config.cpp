#include "Arduino.h"
#include "globals.h"
#include "Config.h"
#include "DataStructures.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <set>

#define DEBUG true  //set to true for debug output, false for no debug output
#define DEBUG_PRINT if(DEBUG)Serial
#define DEBUG_PRINTF if(DEBUG)printf

Config::Config(std::string_view filePath) : FilePath(filePath){

}

void Config::load(){
    if(!LittleFS.exists(FilePath.data())){
        DEBUG_PRINTF("Config file %s not found in filesystem!\n", FilePath.data());
        return;
    }

    File file = LittleFS.open(FilePath.data(), "r");
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        DEBUG_PRINT.print("Failed to load JSON. Error: ");
        DEBUG_PRINT.println(error.f_str());
        return;
    }

    MaxTarget = doc["maxTarget"] | 0;
    HomingTarget = doc["homingTarget"] | 0;
    CANId = doc["canId"] | 1000; // can address id of this hardware
    Debug = doc["debug"] | false;
    POSIXTimezoneString = doc["posixTimezoneString"] | "";

    DEBUG_PRINTF("Max target: %d, Homing Target: %d, Debug: %d\n", MaxTarget, HomingTarget, Debug);
    DEBUG_PRINT.println("Loading Pixel Address array...");

    PixelMap.clear();
    JsonArray cols = doc["pixelMap"].as<JsonArray>();

    ColCount = cols.size();

    if(ColCount == 0){
        DEBUG_PRINT.println("pixelMap array is empty!");
        return;
    }

    RowCount = cols[0].as<JsonArray>().size();

    if(RowCount == 0){
        DEBUG_PRINT.println("pixelMap column is empty!");
        return;
    }

    PixelCount = ColCount * RowCount;

    PixelMap.reserve(PixelCount);

    uint32_t pixelCounter = 1;

    for (JsonVariant col : cols) {
        DEBUG_PRINT.println("Loading column");
        JsonArray rows = col.as<JsonArray>();
        
        for (JsonVariant addr : rows) {
            DEBUG_PRINT.println("Loading row");
            PixelAddress address(addr[0], addr[1]);
            PixelMap.push_back(address);
            DEBUG_PRINTF("Loaded address %d of %d, module %d node %d\n", pixelCounter, PixelCount, address.ModuleId, address.NodeId);
            pixelCounter++;
        }
    }

}

PixelAddress Config::getPixelAddress(uint32_t x, uint32_t y){
    if(x == 0 || y == 0 || x > ColCount || y > RowCount){
        return PixelAddress(0,0); // out of range / broadcast to all addresses
    }

    uint32_t index = ((x - 1) * RowCount) + (y - 1); // calc location in flat array

    return PixelMap[index];
}

uint32_t Config::getRowCount(){
    return RowCount;
}

uint32_t Config::getColCount(){
    return ColCount;
}

uint32_t Config::getMaxTarget(){
    return MaxTarget;
}

uint32_t Config::getHomingTarget(){
    return HomingTarget;
}

uint32_t Config::getCANId(){
    return CANId;
}

std::string_view Config::getPOSIXTimezone(){
    return POSIXTimezoneString;
}

std::string_view Config::getFilePath(){
    return FilePath;
}

std::vector<uint32_t> Config::getUniqueModuleIds(){
    std::set<uint32_t> uniqueModuleIds;

    for(const auto& pixel : PixelMap){
        uniqueModuleIds.insert(pixel.ModuleId);
    }
    
    std::vector<uint32_t> result(uniqueModuleIds.begin(), uniqueModuleIds.end());
    return result;
}

void Config::save(uint8_t *data, size_t len, size_t index, size_t total){
    // Open file for writing (index == 0 means this is the start of the data)
    File file = LittleFS.open(FilePath.data(), (index == 0) ? FILE_WRITE : FILE_APPEND);

    if (file) {
        file.write(data, len);
        file.close();
    }

    // Verify file and reload config
    if (index + len == total) {
        DEBUG_PRINT.println("JSON Settings saved to LittleFS.");
        
        // Optional: Immediately parse it to verify
        File checkFile = LittleFS.open(FilePath.data(), FILE_READ);
        JsonDocument doc;
        deserializeJson(doc, checkFile);
        checkFile.close();

        DEBUG_PRINT.println("Reloading config...");
        load();
        
    }
}