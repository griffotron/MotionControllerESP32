#include "Arduino.h"
#include "globals.h"
#include "MotorControl.h"
#include "SequenceEngine.h"
#include "Config.h"
#include "DataStructures.h"

#define DEBUG true  //set to true for debug output, false for no debug output
#define DEBUG_PRINT if(DEBUG)Serial
#define DEBUG_PRINTF if(DEBUG)printf

MotorControl::MotorControl(Config& config, SequenceEngine& sequenceEngine) : _config(config),  _sequenceEngine(sequenceEngine){
}


SequenceEngine::MotionCue MotorControl::createMoveMotionCue(uint32_t moduleId, uint8_t data[8], uint32_t holdTimeMs)
{
  return SequenceEngine::MotionCue(moduleId, data, holdTimeMs);
}

SequenceEngine::MotionCue MotorControl::createMoveMotionCue(PixelAddress address, uint8_t priority, uint8_t speed, int32_t target, uint32_t holdTimeMs){
  uint8_t messageType = CANMessageType::MoveToTarget; // Motor move command
  uint8_t target0 = target & 0xFF;
  uint8_t target1 = (target >> 8) & 0xFF;
  uint8_t target2 = (target >> 16) & 0xFF;
  uint8_t target3 = (target >> 24) & 0xFF;

  uint8_t data[8] = {messageType, priority, address.NodeId, speed, target0, target1, target2, target3};

  return createMoveMotionCue(address.ModuleId, data, holdTimeMs);
}

SequenceEngine::MotionCue MotorControl::createMoveMotionCue(uint32_t pixelX, uint32_t pixelY, uint8_t priority, uint8_t speed, int32_t target, uint32_t holdTimeMs)
{
  DEBUG_PRINTF("Cols: %d Rows %d Max Target %d", _config.getColCount(), _config.getRowCount(), _config.getMaxTarget());
  PixelAddress address = _config.getPixelAddress(pixelX, pixelY);
  DEBUG_PRINTF("Addressing module: %u node: %u delay(ms): %u\n", address.ModuleId, address.NodeId, holdTimeMs);
  return createMoveMotionCue(address, priority, speed, target, holdTimeMs);
}


SequenceEngine::MotionCue MotorControl::createResetMotorPositionMotionCue(uint32_t pixelX, uint32_t pixelY, uint8_t priority, uint32_t holdTimeMs)
{
  PixelAddress address = _config.getPixelAddress(pixelX, pixelY);
  uint8_t messageType = CANMessageType::SetCurrentPositionToZero; // Motor reset current position to zero
  uint8_t data[8] = {messageType, priority, address.NodeId, 0x0, 0x0, 0x0, 0x0, 0x0}; 
  SequenceEngine::MotionCue motionCue(address.ModuleId, data, holdTimeMs);

  return motionCue;
}

void MotorControl::move(uint32_t pixelX, uint32_t pixelY, uint8_t priority, uint8_t speed, int32_t target){
    _sequenceEngine.inject(createMoveMotionCue(pixelX, pixelY, priority, speed, target, 0));
}

void MotorControl::move(const std::vector<std::array<uint32_t, 2>>& pixelArray, uint8_t priority, uint8_t speed, int32_t target){
    for (const auto& pixelXY : pixelArray) {
      // X = 0, Y = 1
      move(pixelXY[0], pixelXY[1], priority, speed, target);
    }
}

void MotorControl::requestModuleStatus(uint32_t moduleId){
    uint8_t messageType = CANMessageType::ModuleStatusRequest; // Motor move command
    uint32_t controllerAddress = _config.getCANId();
    uint8_t controllerAddress0 = controllerAddress & 0xFF;
    uint8_t controllerAddress1 = (controllerAddress >> 8) & 0xFF;
    uint8_t controllerAddress2 = (controllerAddress >> 16) & 0xFF;
    uint8_t controllerAddress3 = (controllerAddress >> 24) & 0xFF;

    uint8_t data[8] = {messageType, 1, 0, 0, controllerAddress0, controllerAddress1, controllerAddress2, controllerAddress3};
    SequenceEngine::MotionCue motionCue(moduleId, data, 0);
    _sequenceEngine.inject(motionCue);
}



