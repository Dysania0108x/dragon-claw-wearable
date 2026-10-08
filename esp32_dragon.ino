#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>


// ==================================================
// Hardware
// ==================================================

#define MOTOR_PIN 4


// ==================================================
// BLE Settings
// Must match vibration_controller.py
// ==================================================

#define SERVICE_UUID \
  "12345678-1234-1234-1234-123456789abc"

#define CHARACTERISTIC_UUID \
  "87654321-4321-4321-4321-cba987654321"


// ==================================================
// Vibration Modes
// ==================================================

enum VibrationMode {
  STOP,
  WIND,
  RAIN,
  THUNDER
};

VibrationMode currentMode = STOP;


// ==================================================
// PWM Settings
// ==================================================

const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;

// Intensity: 0–255
const int WIND_POWER = 210;
const int RAIN_POWER = 230;
const int THUNDER_POWER = 255;


// ==================================================
// Pattern Timing
// ==================================================

// ---------- WIND ----------
// 3 sec ON → 1.5 sec OFF → repeat

const unsigned long WIND_ON_TIME = 3000;
const unsigned long WIND_OFF_TIME = 1500;


// ---------- RAIN ----------
// 150 ms ON → 120 ms OFF → repeat

const unsigned long RAIN_ON_TIME = 150;
const unsigned long RAIN_OFF_TIME = 120;


// ---------- THUNDER ----------
// 3 strong bursts → 2 sec pause → repeat

const unsigned long THUNDER_BURST_1 = 300;
const unsigned long THUNDER_BURST_2 = 450;
const unsigned long THUNDER_BURST_3 = 600;

const unsigned long THUNDER_GAP = 150;
const unsigned long THUNDER_PAUSE = 2000;


// ==================================================
// Pattern State
// ==================================================

unsigned long previousMillis = 0;
int patternStep = 0;


// ==================================================
// Motor Control
// ==================================================

void setMotor(int power) {
  ledcWrite(MOTOR_PIN, power);
}


void motorOff() {
  ledcWrite(MOTOR_PIN, 0);
}


// ==================================================
// Reset Pattern
// ==================================================

void resetPattern() {

  previousMillis = millis();
  patternStep = 0;

  motorOff();
}


// ==================================================
// Change Mode
// ==================================================

void setMode(VibrationMode newMode) {

  // Do not restart the same pattern
  if (currentMode == newMode) {
    return;
  }

  currentMode = newMode;

  resetPattern();

  switch (newMode) {

    case WIND:
      Serial.println("Mode -> WIND");
      break;

    case RAIN:
      Serial.println("Mode -> RAIN");
      break;

    case THUNDER:
      Serial.println("Mode -> THUNDER");
      break;

    case STOP:
      Serial.println("Mode -> STOP");
      break;
  }
}


// ==================================================
// WIND Pattern
// ==================================================

void updateWind() {

  unsigned long now = millis();

  // Start vibration
  if (patternStep == 0) {

    setMotor(WIND_POWER);

    previousMillis = now;
    patternStep = 1;
  }

  // After 3 sec → OFF
  else if (
    patternStep == 1 &&
    now - previousMillis >= WIND_ON_TIME
  ) {

    motorOff();

    previousMillis = now;
    patternStep = 2;
  }

  // After 1.5 sec OFF → repeat
  else if (
    patternStep == 2 &&
    now - previousMillis >= WIND_OFF_TIME
  ) {

    patternStep = 0;
  }
}


// ==================================================
// RAIN Pattern
// ==================================================

void updateRain() {

  unsigned long now = millis();

  // Start pulse
  if (patternStep == 0) {

    setMotor(RAIN_POWER);

    previousMillis = now;
    patternStep = 1;
  }

  // End pulse
  else if (
    patternStep == 1 &&
    now - previousMillis >= RAIN_ON_TIME
  ) {

    motorOff();

    previousMillis = now;
    patternStep = 2;
  }

  // Short gap → repeat
  else if (
    patternStep == 2 &&
    now - previousMillis >= RAIN_OFF_TIME
  ) {

    patternStep = 0;
  }
}


// ==================================================
// THUNDER Pattern
// ==================================================

void updateThunder() {

  unsigned long now = millis();

  // Burst 1
  if (patternStep == 0) {

    setMotor(THUNDER_POWER);

    previousMillis = now;
    patternStep = 1;
  }

  // End Burst 1
  else if (
    patternStep == 1 &&
    now - previousMillis >= THUNDER_BURST_1
  ) {

    motorOff();

    previousMillis = now;
    patternStep = 2;
  }

  // Gap → Burst 2
  else if (
    patternStep == 2 &&
    now - previousMillis >= THUNDER_GAP
  ) {

    setMotor(THUNDER_POWER);

    previousMillis = now;
    patternStep = 3;
  }

  // End Burst 2
  else if (
    patternStep == 3 &&
    now - previousMillis >= THUNDER_BURST_2
  ) {

    motorOff();

    previousMillis = now;
    patternStep = 4;
  }

  // Gap → Burst 3
  else if (
    patternStep == 4 &&
    now - previousMillis >= THUNDER_GAP
  ) {

    setMotor(THUNDER_POWER);

    previousMillis = now;
    patternStep = 5;
  }

  // End Burst 3
  else if (
    patternStep == 5 &&
    now - previousMillis >= THUNDER_BURST_3
  ) {

    motorOff();

    previousMillis = now;
    patternStep = 6;
  }

  // Long pause → repeat entire sequence
  else if (
    patternStep == 6 &&
    now - previousMillis >= THUNDER_PAUSE
  ) {

    patternStep = 0;
  }
}


// ==================================================
// BLE Characteristic Callback
// Receives W / R / T / S
// ==================================================

class MotorCallback : public BLECharacteristicCallbacks {

  void onWrite(BLECharacteristic *pCharacteristic) override {

    String value = pCharacteristic->getValue();

    if (value.length() == 0) {
      return;
    }

    char command = value[0];

    Serial.print("Received command: ");
    Serial.println(command);


    if (command == 'W') {

      setMode(WIND);

    }

    else if (command == 'R') {

      setMode(RAIN);

    }

    else if (command == 'T') {

      setMode(THUNDER);

    }

    else if (command == 'S') {

      setMode(STOP);

    }
  }
};


// ==================================================
// BLE Server Callback
// Handles connection / disconnection
// ==================================================

class ServerCallbacks : public BLEServerCallbacks {

  void onConnect(BLEServer *pServer) override {

    Serial.println("BLE client connected");
  }


  void onDisconnect(BLEServer *pServer) override {

    Serial.println("BLE client disconnected");

    // Stop vibration immediately
    setMode(STOP);

    // Give BLE stack a moment to finish disconnecting
    delay(100);

    // Start advertising again so Python can reconnect
    BLEDevice::startAdvertising();

    Serial.println("BLE advertising restarted");
  }
};


// ==================================================
// Setup
// ==================================================

void setup() {

  Serial.begin(115200);


  // ------------------------------------------------
  // PWM Motor Setup
  // ESP32 Arduino Core 3.x
  // ------------------------------------------------

  ledcAttach(
    MOTOR_PIN,
    PWM_FREQ,
    PWM_RESOLUTION
  );

  motorOff();


  // ------------------------------------------------
  // BLE Setup
  // ------------------------------------------------

  BLEDevice::init("Museum-ESP32");


  BLEServer *pServer =
      BLEDevice::createServer();


  // Connection / disconnection handling
  pServer->setCallbacks(
    new ServerCallbacks()
  );


  BLEService *pService =
      pServer->createService(
        SERVICE_UUID
      );


  BLECharacteristic *pCharacteristic =
      pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
      );


  pCharacteristic->setCallbacks(
    new MotorCallback()
  );


  pService->start();


  // ------------------------------------------------
  // BLE Advertising
  // ------------------------------------------------

  BLEAdvertising *pAdvertising =
      BLEDevice::getAdvertising();


  pAdvertising->addServiceUUID(
    SERVICE_UUID
  );


  pAdvertising->setScanResponse(true);


  BLEDevice::startAdvertising();


  // ------------------------------------------------
  // Ready
  // ------------------------------------------------

  Serial.println();
  Serial.println("============================");
  Serial.println("Museum ESP32 Ready");
  Serial.println("============================");

  Serial.println("W = Wind");
  Serial.println("R = Rain");
  Serial.println("T = Thunder");
  Serial.println("S = Stop");

  Serial.println();
}


// ==================================================
// Main Loop
// ==================================================

void loop() {

  switch (currentMode) {

    case WIND:
      updateWind();
      break;


    case RAIN:
      updateRain();
      break;


    case THUNDER:
      updateThunder();
      break;


    case STOP:
      motorOff();
      break;
  }


  // Keep BLE responsive
  delay(5);
}