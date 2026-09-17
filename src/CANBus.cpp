#include "Arduino.h"
#include "CANBus.h"
#include "esp_timer.h"
#include "globals.h"
#include "Config.h"

#define DEBUG true  //set to true for debug output, false for no debug output
#define DEBUG_PRINT if(DEBUG)Serial
#define DEBUG_PRINTF if(DEBUG)printf

#define CAN_TX		5
#define CAN_RX		4
#define CAN_SPEED	1000	// kbit/s

CANBus::CANBus(Config& config) : _config(config)
{
}

void CANBus::begin(){

	twai_filter_config_t filterConfig = {
		.acceptance_code = (_config.getCANId() << 21),
		.acceptance_mask = ~(0x7FF << 21), // Only bits in the 11-bit ID range matter
		.single_filter = true
	};

	_sentCounter = 0;
	_receivedCounter = 0;
	_failedCounter = 0;
	_started = ESP32Can.begin(ESP32Can.convertSpeed(CAN_SPEED), CAN_TX, CAN_RX, 10, 10, &filterConfig);
	if(_started) {
		Serial.println("CAN bus started!");
	} else {
		Serial.println("CAN bus failed!");
	}
}

void CANBus::write(uint32_t moduleId, uint8_t* data){
	write(moduleId, data, 0);
}

bool CANBus::read(CanFrame& frame){
	
	if(ESP32Can.readFrame(&frame, 0)){
		uint32_t id = frame.identifier;
		_receivedCounter++;
		DEBUG_PRINTF("Recieved frame with id %d\n", id);
		return true;
	}

	return false;
}

void CANBus::write(uint32_t moduleId, uint8_t* data, uint32_t delayMs){
	CanFrame frame = { 0 };
	frame.identifier = moduleId;
	frame.extd = 0;
	frame.data_length_code = 8;

	for(uint8_t i = 0; i < 8; i++){
		frame.data[i] = data[i];
	}

	DEBUG_PRINTF("Sending to module %u\n", moduleId);

	if(delayMs == 0){
		if(ESP32Can.writeFrame(frame)){
			_sentCounter++;
			DEBUG_PRINTF("... sent successfully!\n");
		}
		else{
			_failedCounter++;
			DEBUG_PRINTF("... failed to write!\n");
		}
	}
	else{
		writeScheduledFrame(frame, delayMs);
	}
}

void CANBus::writeScheduledFrame(CanFrame frame, uint32_t delayMs){

	TimerContext* ctx = new TimerContext();
    ctx->instance = this;
    ctx->frame = frame;

	 const esp_timer_create_args_t timer_args = {
        .callback = &scheduledWrite,
        .arg = (void*)ctx, // Pass any data needed inside the timer
        .name = "scheduled-write"
    };

	esp_timer_create(&timer_args, &ctx->handle); // create timer and pass in the handle so it can delete itself after running
    
    esp_timer_start_once(ctx->handle, (uint64_t)delayMs * 1000); //timer is microseconds
}


void CANBus::scheduledWrite(void *arg){
	TimerContext* ctx = (TimerContext*)arg;

	DEBUG_PRINT.print("Sending scheduled frame");
	if(ctx->instance->ESP32Can.writeFrame(ctx->frame)){  // timeout defaults to 1 ms
		ctx->instance->_sentCounter++;
		DEBUG_PRINT.println("... written successfully!");
	}
	else{
		ctx->instance->_failedCounter++;
		DEBUG_PRINT.println("... failed to write!");
	}

	esp_timer_delete(ctx->handle); //tidy up timer and delete context reference
	delete ctx; 
}

bool CANBus::started(){
	return _started;
}

uint16_t CANBus::speedKbps(){
	return CAN_SPEED;
}

uint32_t CANBus::sentCounter(){
	return _sentCounter;
}
uint32_t CANBus::receivedCounter(){
	return _receivedCounter;
}

uint32_t CANBus::failedCounter(){
	return _failedCounter;
}

