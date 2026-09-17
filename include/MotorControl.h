
// MotorControl.h
#pragma once
#include "Config.h"
#include "SequenceEngine.h"
#include "DataStructures.h"

#ifndef _MOTORCONTROL_h
#define _MOTORCONTROL_h

class MotorControl{
	private:
		SequenceEngine& _sequenceEngine;
		Config& _config;

	public: 
		MotorControl(Config& config, SequenceEngine& sequenceEngine);
        void move(uint32_t pixelX, uint32_t pixelY, uint8_t priority, uint8_t speed, int32_t target);
		void move(const std::vector<std::array<uint32_t, 2>>& pixelArray, uint8_t priority, uint8_t speed, int32_t target);
		SequenceEngine::MotionCue createMoveMotionCue(uint32_t moduleId, uint8_t data[8], uint32_t holdTimeMs);
		SequenceEngine::MotionCue createMoveMotionCue(uint32_t pixelX, uint32_t pixelY, uint8_t priority, uint8_t speed, int32_t target, uint32_t holdTimeMs);
		SequenceEngine::MotionCue createMoveMotionCue(PixelAddress address, uint8_t priority, uint8_t speed, int32_t target, uint32_t holdTimeMs);
		SequenceEngine::MotionCue createResetMotorPositionMotionCue(uint32_t pixelX, uint32_t pixelY, uint8_t priority, uint32_t holdTimeMs);		
		void requestModuleStatus(uint32_t moduleId);
};

#endif



