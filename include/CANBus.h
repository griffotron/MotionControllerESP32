// CANBus.h
#pragma once

#include <atomic>
#include <ESP32-TWAI-CAN.hpp>
#include "Config.h"

#ifndef _CANBUS_h
#define _CANBUS_h

class CANBus{
	private:
		TwaiCAN ESP32Can;
		static void scheduledWrite(void *arg);
		bool _started = false;
		Config& _config;
		// Incremented from both the caller's task and the esp_timer task
		// (scheduledWrite), so these must be atomic to avoid lost counts.
		std::atomic<uint32_t> _sentCounter{0};
		std::atomic<uint32_t> _receivedCounter{0};
		std::atomic<uint32_t> _failedCounter{0};

		struct TimerContext {
			CANBus* instance; // "this" pointer
			CanFrame frame; 
			esp_timer_handle_t handle;
		};

	public: 
        CANBus(Config& config);
		void begin();
		bool read(CanFrame& frame);
		void write(uint32_t moduleId, uint8_t* data);
		void write(uint32_t moduleId, uint8_t* data, uint32_t delayMs);
		void writeScheduledFrame(CanFrame frame, uint32_t delayMs);
		bool started();
		uint16_t speedKbps();
		uint32_t sentCounter();
		uint32_t receivedCounter();
		uint32_t failedCounter();
		
};

#endif

