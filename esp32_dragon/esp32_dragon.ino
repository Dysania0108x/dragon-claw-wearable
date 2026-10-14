#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

// ==================================================
// 1. Hardware Configuration
// ==================================================

#define MOTOR_PIN 4

// ==================================================
// 2. BLE Configuration
// Must match vibration_controller.py
// ==================================================

#define SERVICE_UUID \
  "12345678-1234-1234-1234-123456789abc"

#define CHARACTERISTIC_UUID \
  "87654321-4321-4321-4321-cba987654321"

#define DEVICE_NAME "Museum-ESP32"

// ==================================================
// 3. Vibration Modes
// ==================================================

enum VibrationMode {
  STOP,
  WIND,
  RAIN,
  THUNDER
};

VibrationMode currentMode = STOP;

// ==================================================
// 4. PWM Configuration
// ==================================================

const int PWM_FREQ = 5000;
const int PWM_RESOLUTION = 8;

// Intensity range: 0-255

// WIND: gradual intensity
const int WIND_MIN_POWER = 70;
const int WIND_MAX_POWER = 110;

// RAIN: medium intensity
const int RAIN_POWER = 100;

// THUNDER: strong intensity
const int THUNDER_POWER = 250;

// ==================================================
// 5. Pattern Timing
// ==================================================

// ---------------- WIND ----------------
// Gradual increase -> decrease -> pause

const unsigned long WIND_RISE_TIME = 1500;
const unsigned long WIND_FALL_TIME = 1500;
const unsigned long WIND_PAUSE_TIME = 1500;

// ---------------- RAIN ----------------
// Light -> Moderate -> Heavy -> Repeat

const unsigned long RAIN_ON_TIME = 150;

const unsigned long RAIN_OFF_TIMES[] = {
  350,   // Light Rain
  180,   // Moderate Rain
  80     // Heavy Rain
};

const int RAIN_STAGES = 3;

const unsigned long RAIN_STAGE_DURATION = 5000;

// ---------------- THUNDER ----------------
// Three irregular bursts -> long pause

const unsigned long THUNDER_BURST_1 = 300;
const unsigned long THUNDER_BURST_2 = 450;
const unsigned long THUNDER_BURST_3 = 600;

const unsigned long THUNDER_GAP_1 = 200;
const unsigned long THUNDER_GAP_2 = 120;

const unsigned long THUNDER_PAUSE = 2000;

// ==================================================
// 6. Pattern State
// ==================================================

unsigned long previousMillis = 0;
unsigned long rainStartMillis = 0;

int patternStep = 0;

// ==================================================
// 7. Motor Control
// ==================================================

void setMotor(int power) {

  power = constrain(power, 0, 255);

  ledcWrite(MOTOR_PIN, power);
}

void motorOff() {

  ledcWrite(MOTOR_PIN, 0);
}

// ==================================================
// 8. Reset Pattern
// ==================================================

void resetPattern() {

  previousMillis = millis();
  rainStartMillis = millis();

  patternStep = 0;

  motorOff();
}

// ==================================================
// 9. Change Mode
// ==================================================

void setMode(VibrationMode newMode) {

  // Avoid restarting the same pattern
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
// 10. WIND Pattern
// Gradual increase -> decrease -> pause
// ==================================================

void updateWind() {

  unsigned long now = millis();

  unsigned long elapsed = now - previousMillis;

  // Step 0: Gradual increase
  if (patternStep == 0) {

    if (elapsed >= WIND_RISE_TIME) {

      setMotor(WIND_MAX_POWER);

      previousMillis = now;
      patternStep = 1;

    } else {

      int power = map(
        elapsed,
        0,
        WIND_RISE_TIME,
        WIND_MIN_POWER,
        WIND_MAX_POWER
      );

      setMotor(power);
    }
  }

  // Step 1: Gradual decrease
  else if (patternStep == 1) {

    if (elapsed >= WIND_FALL_TIME) {

      motorOff();

      previousMillis = now;
      patternStep = 2;

    } else {

      int power = map(
        elapsed,
        0,
        WIND_FALL_TIME,
        WIND_MAX_POWER,
        WIND_MIN_POWER
      );

      setMotor(power);
    }
  }

  // Step 2: Pause
  else if (patternStep == 2) {

    if (elapsed >= WIND_PAUSE_TIME) {

      previousMillis = now;
      patternStep = 0;
    }
  }
}

// ==================================================
// 11. RAIN Pattern
// Increasing pulse frequency
// ==================================================

void updateRain() {

  unsigned long now = millis();

  unsigned long elapsed = now - rainStartMillis;

  // Determine current rain stage
  int stage =
    (elapsed / RAIN_STAGE_DURATION) % RAIN_STAGES;

  unsigned long offTime = RAIN_OFF_TIMES[stage];

  // Step 0: Start pulse
  if (patternStep == 0) {

    setMotor(RAIN_POWER);

    previousMillis = now;
    patternStep = 1;
  }

  // Step 1: End pulse
  else if (
    patternStep == 1 &&
    now - previousMillis >= RAIN_ON_TIME
  ) {

    motorOff();

    previousMillis = now;
    patternStep = 2;
  }

  // Step 2: Wait before next pulse
  else if (
    patternStep == 2 &&
    now - previousMillis >= offTime
  ) {

    patternStep = 0;
  }
}

// ==================================================
// 12. THUNDER Pattern
// Irregular strong bursts
// ==================================================

void updateThunder() {

  unsigned long now = millis();

  switch (patternStep) {

    // Burst 1
    case 0:

      setMotor(THUNDER_POWER);

      previousMillis = now;
      patternStep = 1;

      break;

    // End Burst 1
    case 1:

      if (
        now - previousMillis >= THUNDER_BURST_1
      ) {

        motorOff();

        previousMillis = now;
        patternStep = 2;
      }

      break;

    // Gap 1
    case 2:

      if (
        now - previousMillis >= THUNDER_GAP_1
      ) {

        setMotor(THUNDER_POWER);

        previousMillis = now;
        patternStep = 3;
      }

      break;

    // End Burst 2
    case 3:

      if (
        now - previousMillis >= THUNDER_BURST_2
      ) {

        motorOff();

        previousMillis = now;
        patternStep = 4;
      }

      break;

    // Gap 2
    case 4:

      if (
        now - previousMillis >= THUNDER_GAP_2
      ) {

        setMotor(THUNDER_POWER);

        previousMillis = now;
        patternStep = 5;
      }

      break;

    // End Burst 3
    case 5:

      if (
        now - previousMillis >= THUNDER_BURST_3
      ) {

        motorOff();

        previousMillis = now;
        patternStep = 6;
      }

      break;

    // Long pause
    case 6:

      if (
        now - previousMillis >= THUNDER_PAUSE
      ) {

        patternStep = 0;
      }

      break;
  }
}

// ==================================================
// 13. BLE Characteristic Callback
// Receives W / R / T / S
// ==================================================

class MotorCallback : public BLECharacteristicCallbacks {

  void onWrite(
    BLECharacteristic *pCharacteristic
  ) override {

    String value = pCharacteristic->getValue();

    if (value.length() == 0) {
      return;
    }

    char command = value[0];

    Serial.print("Received command: ");
    Serial.println(command);

    if (command == 'W') {

      setMode(WIND);

    } else if (command == 'R') {

      setMode(RAIN);

    } else if (command == 'T') {

      setMode(THUNDER);

    } else if (command == 'S') {

      setMode(STOP);
    }
  }
};

// ==================================================
// 14. BLE Server Callbacks
// ==================================================

class ServerCallbacks : public BLEServerCallbacks {

  void onConnect(
    BLEServer *pServer
  ) override {

    Serial.println("BLE client connected");
  }

  void onDisconnect(
    BLEServer *pServer
  ) override {

    Serial.println("BLE client disconnected");

    // Stop vibration
    setMode(STOP);

    delay(100);

    // Restart advertising
    BLEDevice::startAdvertising();

    Serial.println("BLE advertising restarted");
  }
};

// ==================================================
// 15. Setup
// ==================================================

void setup() {

  Serial.begin(115200);

  // ---------------- PWM Setup ----------------

  ledcAttach(
    MOTOR_PIN,
    PWM_FREQ,
    PWM_RESOLUTION
  );

  motorOff();

  // ---------------- BLE Setup ----------------

  BLEDevice::init(DEVICE_NAME);

  BLEServer *pServer =
    BLEDevice::createServer();

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

  // ---------------- Advertising ----------------

  BLEAdvertising *pAdvertising =
    BLEDevice::getAdvertising();

  pAdvertising->addServiceUUID(
    SERVICE_UUID
  );

  pAdvertising->setScanResponse(true);

  BLEDevice::startAdvertising();

  // ---------------- Ready ----------------

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
// 16. Main Loop
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