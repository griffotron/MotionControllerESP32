/*
Copyright (c) 2026 GRIFFOTRON
https://github.com/griffotron
https://www.youtube.com/@griffotron
*/
#include <Arduino.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <AsyncJson.h>
#if __has_include("ArduinoJson.h")
  #include <ArduinoJson.h>
  #include <AsyncJson.h>
#endif
#include <LittleFS.h>
#include "wifi_creds.h"
#include <FastLED.h>
#include <set>

#include <ESP32-TWAI-CAN.hpp>
#include "globals.h"
#include "CANBus.h"
#include "HttpHandlers.h"
#include "WebSocketHandler.h"
#include "MotorControl.h"
#include "SequenceEngine.h"
#include "SequenceGenerator.h"
#include "Config.h"
#include "SystemManager.h"
#include "DataStructures.h"
#include "RgbButton.h"

#define DEBUG true  //set to true for debug output, false for no debug output
#define DEBUG_PRINT if(DEBUG)Serial
#define DEBUG_PRINTF if(DEBUG)printf

#define LED_PIN 38

#define WIFI_BTN_PIN 3
#define WIFI_BTN_RED_LED_PIN 17
#define WIFI_BTN_GRN_LED_PIN 18
#define WIFI_BTN_BLU_LED_PIN 8

#define ACTIVATE_BTN_PIN 13
#define ACTIVATE_BTN_RED_LED_PIN 10
#define ACTIVATE_BTN_GRN_LED_PIN 11
#define ACTIVATE_BTN_BLU_LED_PIN 12

bool clockMode = false;              // true while the clock sequence is the active sequence
bool demoMode = false;               // true while demo mode is cycling random sequences + the clock
uint32_t demoIntervalMs = 5u * 60u * 1000u; // clock display window between demo sequences (default 5 min)
bool demoRestart = false;            // set by the /demo handler to (re)start the cycle

// ---- Buttony stuff ----
// Hold either button this long to trigger its "hold" action (WiFi: reset ESP32,
// Activate: re-run the module test).
const uint32_t BUTTON_HOLD_MS = 3000;
const uint32_t WIFI_FLASH_MS = 500;      // WiFi button flashes blue during startup
const uint32_t ACTIVATE_FLASH_MS = 1000; // Activate button long flash blue on start

RgbButton wifiButton(WIFI_BTN_PIN, WIFI_BTN_RED_LED_PIN, WIFI_BTN_GRN_LED_PIN, WIFI_BTN_BLU_LED_PIN);
RgbButton activateButton(ACTIVATE_BTN_PIN, ACTIVATE_BTN_RED_LED_PIN, ACTIVATE_BTN_GRN_LED_PIN, ACTIVATE_BTN_BLU_LED_PIN);

/* Activate button (the top one): press once for module test, then should go to steady green
if all modules responded. Goes red if a module doesnt respond. Red could be because the config is looking for a module that doesn't
exist. Or you need to check the wiring. Or, maybe, you've blown something up :-( */
enum class ActivateState : uint8_t { WaitingFirstActivation, Testing, Ready, Failed };
ActivateState activateState = ActivateState::WaitingFirstActivation;
bool activateHoldFired = false;

// Module testing
std::set<uint32_t> respondedModules;   // distinct module ids that answered this test
size_t expectedModuleCount = 0;        // modules we asked (from config)
uint32_t moduleTestStart = 0;
uint32_t moduleTestTimeoutMs = 0;

// Short-pressing the Activate button (once Ready) steps through these showcase modes.
enum class Showcase : uint8_t { Demo, Clock, Idle };
Showcase showcaseNext = Showcase::Demo;

CRGB leds[1];
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

int num = 0;
Config config = Config("/config.json");
CANBus CAN = CANBus(config);
SystemManager systemMgr = SystemManager(config, CAN);
SequenceEngine sequenceEngine = SequenceEngine(CAN);
MotorControl motorControl = MotorControl(config, sequenceEngine);
SequenceGenerator sequenceGenerator = SequenceGenerator(config, motorControl);
HttpHandlers httpHandlers = HttpHandlers(config, server, motorControl, sequenceEngine, sequenceGenerator, systemMgr);
WebSocketHandler wsHandler = WebSocketHandler(server, ws, config, systemMgr, sequenceEngine);

void setStatusColor(CRGB color) {
    leds[0] = color;
    FastLED.show();
}

void setStatusLEDToDefault(){
  if(WiFi.status() == WL_CONNECTED){
    setStatusColor(CRGB::Green);
  }
  else{
    setStatusColor(CRGB::Red);
  }
}

// Clear the display (home all motors) before a new sequence takes over, so it doesn't slowly paint over whatever was showing.
// Pausing first stops the outgoing sequence's timer re-opening pixels mid-clear. Mirrors the HTTP handlers.
void clearAndLoad(const SequenceEngine::Sequence& seq){
  sequenceEngine.pause();
  sequenceEngine.inject(motorControl.createMoveMotionCue(0, 0, 1, 80, 0, 0));
  sequenceEngine.load(seq);
}

// -- Showcase modes stepped through by bashing the Activate button --
// These reproduce what the /demo and /sequence/generate HTTP handlers do so the button and the web UI drive the same behaviour.
void startDemoShowcase(){
  demoIntervalMs = 5u * 60u * 1000u; // default 5 min clock window, same as the /demo default
  clockMode = false;   // demo owns the display; its own loop keeps the clock ticking
  demoRestart = true;  // signals the main loop to (re)start the cycle
  demoMode = true;
  DEBUG_PRINT.println("Activate: showcase -> Demo");
}

void startClockShowcase(){
  demoMode = false;
  clockMode = true;
  clearAndLoad(sequenceGenerator.displayCurrentTime());
  DEBUG_PRINT.println("Activate: showcase -> Clock");
}

void startIdleShowcase(){
  demoMode = false;
  clockMode = false;
  clearAndLoad(sequenceGenerator.idle());
  DEBUG_PRINT.println("Activate: showcase -> Idle");
}

void cycleShowcase(){
  switch(showcaseNext){
    case Showcase::Demo:  startDemoShowcase();  showcaseNext = Showcase::Clock; break;
    case Showcase::Clock: startClockShowcase(); showcaseNext = Showcase::Idle;  break;
    case Showcase::Idle:  startIdleShowcase();  showcaseNext = Showcase::Demo;  break;
  }
}

// Here we're asking modules to "check in", by firing of a CAN message, and hopefully
// it'll come back saying "I'm here! Witness me! WITNESS ME!!" or something like that
// Each response makes the button blink green so if we have fast eyes we can count the flashes to see how many modules
// responded. If all responsd, button goes solid green. Nice. If one or more doesn't respond
// button goes red. Which is bad, and probably because of my dodgy wiring.
void startModuleTest(){
  expectedModuleCount = config.getUniqueModuleIds().size();
  respondedModules.clear();
  moduleTestStart = millis();
  // Requests are sent 200ms apart, so allow for that plus a round-trip margin.
  moduleTestTimeoutMs = expectedModuleCount * 200u + 2000u;

  DEBUG_PRINTF("Activate: starting module test (%u module(s))\n", expectedModuleCount);

  if (expectedModuleCount == 0) {
    // Nothing configured to test - treat as passed.
    activateState = ActivateState::Ready;
    activateButton.setSolid(RgbButton::Color::Green);
    return;
  }

  activateState = ActivateState::Testing;
  activateButton.setSolid(RgbButton::Color::Off); // dark base; each response blinks green
  systemMgr.scanModules();
}

// Called from loop() for every ModuleStatusResponse CAN frame received.
void handleModuleResponse(const CanFrame& frame){
  if (activateState != ActivateState::Testing) return;

  uint32_t moduleId = (uint32_t)frame.data[4]
                    | ((uint32_t)frame.data[5] << 8)
                    | ((uint32_t)frame.data[6] << 16)
                    | ((uint32_t)frame.data[7] << 24);
  respondedModules.insert(moduleId);
  activateButton.blinkOnce(RgbButton::Color::Green, 120); // flash once per response

  if (respondedModules.size() >= expectedModuleCount) {
    activateState = ActivateState::Ready;
    activateButton.setSolid(RgbButton::Color::Green);
    DEBUG_PRINT.println("Activate: all modules responded -> Ready (green)");
  }
}

void setup() {
  DEBUG_PRINT.begin(115200);
  DEBUG_PRINT.println("Booted");

  FastLED.addLeds<WS2812B, LED_PIN>(leds, 1);
  FastLED.setBrightness(20);

  setStatusColor(CRGB::Blue); // blue LED for booting

  // Buttons:
  // Activate (upper button) long-flashes blue and waits for its first activation.
  // WiFi (lower button) flashes blue while connecting. Goes red if failed. Hold to reset
  wifiButton.begin();
  activateButton.begin();
  wifiButton.setFlash(RgbButton::Color::Blue, WIFI_FLASH_MS);
  activateButton.setFlash(RgbButton::Color::Blue, ACTIVATE_FLASH_MS);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

  DEBUG_PRINT.println("Connecting to WiFi...");
  unsigned long startAttemptTime = millis();

  // Wait for connection with a 10-second timeout. update() the buttons so their
  // flash effects keep animating during this blocking wait.
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    wifiButton.update();
    activateButton.update();
    delay(10);
    DEBUG_PRINT.print(".");
  }

  setStatusLEDToDefault();

  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINT.println("\nConnected!");
    wifiButton.setSolid(RgbButton::Color::Green); // solid green once connected
  } else {
    DEBUG_PRINT.println("\nFailed to connect.");
    wifiButton.setSolid(RgbButton::Color::Blue);  // solid blue on failure
  }

  DEBUG_PRINT.print("IP Address: ");
  DEBUG_PRINT.println(WiFi.localIP());

  LittleFS.begin();

  MDNS.begin(MDNSName);
  pinMode(BUILTIN_LED, OUTPUT);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET,PUT,POST");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  
  config.load();

  // Set the time using timezone from config - lookup the POSIX timezone string here: https://github.com/nayarsystems/posix_tz_db
  configTzTime(config.getPOSIXTimezone().data(), "pool.ntp.org", "time.nist.gov"); 
  DEBUG_PRINT.println("NTP sync requested");

  wsHandler.setup();
  httpHandlers.setup();

  server.begin();
  CAN.begin();
  sequenceEngine.begin();
}


void logMemory() {
    // Total free memory
    size_t freeHeap = ESP.getFreeHeap();
    
    // The largest single block available (critical for std::vector allocations)
    size_t maxBlock = ESP.getMaxAllocHeap();

    uint32_t minFreeHeap = ESP.getMinFreeHeap();

    Serial.printf("Free Heap: %u bytes | Max Block: %u bytes | Min free heap: %u bytes\n", freeHeap, maxBlock, minFreeHeap);

    if (psramFound()) {
      DEBUG_PRINT.printf("Free PSRAM: %u\n", ESP.getFreePsram());
    }
}

void checkWsClientConnected(){
    ws.cleanupClients();
    if(ws.count() > 0){
      if(leds[0] == CRGB::GreenYellow){
        setStatusColor(CRGB::Green);
      }
      else{
        setStatusColor(CRGB::GreenYellow);
      }
    }
    else{
      setStatusLEDToDefault();
    }
}


uint32_t lastMemoryLog = 0;
const uint32_t logInterval = 30000; // milliseconds
uint32_t lastWsCheck = 0;
const uint32_t wsCheckInterval = 1000; // milliseconds
uint32_t lastClockCheck = 0;
const uint32_t clockCheckInterval = 1000; // milliseconds (poll for clock sequence)
uint32_t lastDemoCheck = 0;
const uint32_t demoCheckInterval = 1000; // milliseconds, evaluate demo state frequency
enum DemoPhase : uint8_t { DEMO_SEQUENCE, DEMO_CLOCK };
DemoPhase demoPhase = DEMO_CLOCK;
uint32_t demoClockStart = 0;         // millis() when the current clock window began
uint32_t lastStatusCheck = 0;
const uint32_t statusCheckInterval = 100; // poll for sequence status changes 10x/second
CanFrame frame;


void loop() {


  if (CAN.read(frame)) {
    uint8_t type = frame.data[0];
    if(type == CANMessageType::ModuleStatusResponse){
        wsHandler.broadcastModuleStatus(frame);
        handleModuleResponse(frame); // drive the Activate button's module test
    }
  }

  uint32_t currentTime = millis();

  // Check if 30 seconds have passed
  if (currentTime - lastMemoryLog >= logInterval) {
      lastMemoryLog = currentTime; // Update the timer
      logMemory();
  }

  //Web socket tidy up and led feedback
  if (currentTime - lastWsCheck >= wsCheckInterval) {
      lastWsCheck = currentTime;
      checkWsClientConnected();
  }

  // Clock auto-reload: when the clock sequence finishes (at the minute boundary), regenerate it for the new current time and load it immediately.
  // displayCurrentTime() recalculates the hold time from the current second, so each reload re-syncs itself to the top of the next minute.
  if (clockMode && currentTime - lastClockCheck >= clockCheckInterval) {
      lastClockCheck = currentTime;
      // The engine latches paused=true when the (non-looping) clock sequence reaches its end (i.e. once the minute-boundary hold has elapsed).
      // That finish is our cue to regenerate the display for the new minute.
      if (sequenceEngine.getStatus().paused) {
          sequenceEngine.load(sequenceGenerator.displayCurrentTime());
      }
  }

  // Demo mode: show a random saved sequence, then the clock for demoIntervalMs, then another random saved sequence, and so on.
  // The whole cycle lives here (rather than reusing clockMode) so the state machine is in one place.
  if (demoMode && currentTime - lastDemoCheck >= demoCheckInterval) {
      lastDemoCheck = currentTime;

      if (demoRestart) {
          // Kick off (or restart) the cycle with a fresh random saved sequence.
          demoRestart = false;
          SequenceEngine::Sequence seq = sequenceGenerator.loadRandomSaved();
          if (seq.playlist.empty()) {
              // Nothing saved yet, just run the clock and re-check next window.
              sequenceEngine.load(sequenceGenerator.displayCurrentTime());
              demoClockStart = currentTime;
              demoPhase = DEMO_CLOCK;
          } else {
              clearAndLoad(seq);
              demoPhase = DEMO_SEQUENCE;
          }
      }
      else if (demoPhase == DEMO_SEQUENCE) {
          // Wait for the sequence to finish, then show the clock.
          if (sequenceEngine.getStatus().paused) {
              sequenceEngine.load(sequenceGenerator.displayCurrentTime());
              demoClockStart = currentTime;
              demoPhase = DEMO_CLOCK;
          }
      }
      else { // DEMO_CLOCK
          if (currentTime - demoClockStart >= demoIntervalMs) {
              // Window elapsed, show another random saved sequence.
              SequenceEngine::Sequence seq = sequenceGenerator.loadRandomSaved();
              if (seq.playlist.empty()) {
                  // Still nothing saved, keep the clock and restart the window.
                  sequenceEngine.load(sequenceGenerator.displayCurrentTime());
                  demoClockStart = currentTime;
              } else {
                  clearAndLoad(seq);
                  demoPhase = DEMO_SEQUENCE;
              }
          }
          else if (sequenceEngine.getStatus().paused) {
              // Clock hit the minute boundary, refresh it (same as clock mode).
              sequenceEngine.load(sequenceGenerator.displayCurrentTime());
          }
      }
  }

  // Keep UI updated with current status of sequence (play pause buttons etc). Poll here so sequence engine timer doesn't get delayed
  if (currentTime - lastStatusCheck >= statusCheckInterval) {
      lastStatusCheck = currentTime;
      if (sequenceEngine.consumeStatusChanged()) {
          wsHandler.broadcastSequenceStatus(sequenceEngine.getStatus());
      }
  }

  // -- WiFi button (but alos resets controller) ---
  // Press-and-hold turns it solid red and resets the ESP32 after BUTTON_HOLD_MS. If seeing it flashing red makes you chicken out
  // and release it early, it'll go back to the normal connection-status colour.
  wifiButton.update();
  if (wifiButton.pressed()) {
    wifiButton.setSolid(RgbButton::Color::Red); // red while held, until reset
  }
  if (wifiButton.isDown() && wifiButton.heldForMs() >= BUTTON_HOLD_MS) {
    DEBUG_PRINT.println("WiFi button held 3s -> restarting ESP32");
    delay(50); // let the serial line flush
    ESP.restart();
  }
  if (wifiButton.released()) {
    wifiButton.setSolid(WiFi.status() == WL_CONNECTED ? RgbButton::Color::Green : RgbButton::Color::Blue);
  }

  // --Activate button--
  activateButton.update();

  if (activateButton.pressed()) {
    activateHoldFired = false; // arm the hold detector for this new press
  }

  // Press-and-hold (re)runs the module test. The latch makes sure it fires just once per hold, not every loop.
  if (!activateHoldFired && activateButton.isDown() && activateState != ActivateState::Testing && activateButton.heldForMs() >= BUTTON_HOLD_MS) {
    activateHoldFired = true;
    startModuleTest();
  }

  // Short press: kick off the first test, or (once Ready) step the showcase.
  if (activateButton.released() && !activateHoldFired && activateButton.lastPressDurationMs() < BUTTON_HOLD_MS) {
    switch (activateState) {
      case ActivateState::WaitingFirstActivation:
        startModuleTest();                 // first activation runs the test
        break;
      case ActivateState::Ready:
        cycleShowcase();                   // green + short press cycles modes
        break;
      default:
        break;                             // Failed: only a 3s hold retests
    }
  }

  // Module test timeout: not everyone answered in time -> solid red.
  if (activateState == ActivateState::Testing && millis() - moduleTestStart >= moduleTestTimeoutMs) {
    activateState = ActivateState::Failed;
    activateButton.setSolid(RgbButton::Color::Red);
    DEBUG_PRINTF("Activate: module test timed out (%u/%u responded) -> Failed (red)\n",
                 respondedModules.size(), expectedModuleCount);
  }
}

