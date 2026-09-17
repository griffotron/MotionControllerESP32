#pragma once
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "DataStructures.h"

#ifndef _CONFIG_h
#define _CONFIG_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif


#define MDNSName "motioncontroller"

class Config{
	public: 
		Config(std::string_view filePath);
        PixelAddress getPixelAddress(uint32_t x, uint32_t y);
		void load();
		void save(uint8_t *data, size_t len, size_t index, size_t total);
		void save(JsonVariantConst& doc);
		uint32_t getRowCount();
		uint32_t getColCount();
		uint32_t getMaxTarget();
		uint32_t getHomingTarget();
		uint32_t getCANId();
		std::string_view getPOSIXTimezone();
		std::string_view getFilePath();
		std::vector<uint32_t> getUniqueModuleIds();

	private:
		int32_t MaxTarget;
		int32_t HomingTarget;
		uint32_t RowCount;
		uint32_t ColCount;
		uint32_t PixelCount;
		uint32_t CANId;
		std::string POSIXTimezoneString;
		bool Debug;
		std::string_view FilePath;
		std::vector<PixelAddress> PixelMap;
		
};

#endif



