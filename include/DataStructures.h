
#ifndef _DATASTRUCTS_h
#define _DATASTRUCTS_h

struct CANMessage {
    uint32_t AddressId; // Id of the Module
	byte Type; 		// byte 1 of CAN message
	byte Priority; 	// byte 2 of CAN message
	std::array<uint8_t, 6> Data; 	// byte 3-8 of CAN message
};

class CANMessageType
{
	public:
	static const byte MoveToTarget = 0x1;
	static const byte MoveToHome = 0x2;
	static const byte SetCurrentPositionToZero = 0x3;
	static const byte CancelLastCommand = 0x4;
	static const byte EmergencyStop = 0x5;
	static const byte EmergencyStopReset = 0x6;
	static const byte Stats = 0x7;
	static const byte FullReset = 0x8;
	static const byte ModuleStatusRequest = 0x9;
	static const byte ModuleStatusResponse = 0x0A;

};


struct PixelAddress {
	PixelAddress(uint32_t moduleId, uint8_t nodeId): ModuleId(moduleId), NodeId(nodeId) {}
	PixelAddress() = default;
	uint32_t ModuleId; // Id of the Module
	uint8_t NodeId; // Id of engine
};


#endif