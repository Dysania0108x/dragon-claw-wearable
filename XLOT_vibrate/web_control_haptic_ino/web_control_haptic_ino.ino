#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

// ==================================================
// 1. Hardware Configuration
// ==================================================

#define MOTOR_PIN 17

// ==================================================
// 2. Wi-Fi Configuration
// ==================================================

const char* AP_SSID = "hxever";
const char* AP_PASS = "hank_hsu";

WebServer server(80);

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
// 4. Intensity Configuration (0-100%)
// ==================================================

// Wind: gradual increase and decrease
const int WIND_MIN_POWER = 35;
const int WIND_MAX_POWER = 55;

// Rain: medium intensity
const int RAIN_POWER = 65;

// Thunder: strong intensity
const int THUNDER_POWER = 90;

// ==================================================
// 5. Pattern Timing (milliseconds)
// ==================================================

// WIND
const unsigned long WIND_RISE_TIME = 1500;
const unsigned long WIND_FALL_TIME = 1500;
const unsigned long WIND_PAUSE_TIME = 1500;

// RAIN
const unsigned long RAIN_ON_TIME = 150;

const unsigned long RAIN_OFF_TIMES[] = {
  850,  // Light: 1 pulse/sec
  450,  // Moderate
  200   // Heavy
};

const int RAIN_STAGES = 3;
const unsigned long RAIN_STAGE_DURATION = 5000;

// THUNDER
const unsigned long THUNDER_BURST_1 = 300;
const unsigned long THUNDER_BURST_2 = 450;
const unsigned long THUNDER_BURST_3 = 600;

const unsigned long THUNDER_GAP_1 = 200;
const unsigned long THUNDER_GAP_2 = 120;
const unsigned long THUNDER_PAUSE = 2000;

// ==================================================
// 6. State & Safety
// ==================================================

unsigned long previousMillis = 0;
unsigned long rainStartMillis = 0;

int patternStep = 0;

// Stop vibration if no heartbeat is received
const unsigned long COMMAND_TIMEOUT = 10000;

unsigned long lastHeartbeat = 0;

// ==================================================
// 7. Motor Control
// ==================================================

void setMotor(int percent) {

  percent = constrain(percent, 0, 100);

  int duty = (percent * 255UL) / 100;

  analogWrite(MOTOR_PIN, duty);
}

void motorOff() {
  analogWrite(MOTOR_PIN, 0);
}

// ==================================================
// 8. Pattern Reset
// ==================================================

void resetPattern() {

  previousMillis = millis();
  rainStartMillis = millis();

  patternStep = 0;

  motorOff();
}

// ==================================================
// 9. Mode Control
// ==================================================

void setMode(VibrationMode newMode) {

  // Refresh watchdog
  lastHeartbeat = millis();

  // Do not restart the same pattern
  if (currentMode == newMode) {
    return;
  }

  currentMode = newMode;

  resetPattern();

  Serial.print("Mode -> ");

  switch (newMode) {

    case WIND:
      Serial.println("WIND");
      break;

    case RAIN:
      Serial.println("RAIN");
      break;

    case THUNDER:
      Serial.println("THUNDER");
      break;

    case STOP:
      Serial.println("STOP");
      break;
  }
}

// ==================================================
// 10. WIND
// Gradual increase -> decrease -> pause -> repeat
// ==================================================

void updateWind() {

  unsigned long now = millis();
  unsigned long elapsed = now - previousMillis;

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

  else if (patternStep == 2) {

    if (elapsed >= WIND_PAUSE_TIME) {

      previousMillis = now;
      patternStep = 0;
    }
  }
}

// ==================================================
// 11. RAIN
// Light -> Moderate -> Heavy -> Repeat
// ==================================================

void updateRain() {

  unsigned long now = millis();

  unsigned long elapsed = now - rainStartMillis;

  int stage =
    (elapsed / RAIN_STAGE_DURATION) % RAIN_STAGES;

  unsigned long offTime = RAIN_OFF_TIMES[stage];

  if (patternStep == 0) {

    setMotor(RAIN_POWER);

    previousMillis = now;
    patternStep = 1;
  }

  else if (
    patternStep == 1 &&
    now - previousMillis >= RAIN_ON_TIME
  ) {

    motorOff();

    previousMillis = now;
    patternStep = 2;
  }

  else if (
    patternStep == 2 &&
    now - previousMillis >= offTime
  ) {

    patternStep = 0;
  }
}

// ==================================================
// 12. THUNDER
// Three irregular bursts -> pause -> repeat
// ==================================================

void updateThunder() {

  unsigned long now = millis();

  switch (patternStep) {

    case 0:

      setMotor(THUNDER_POWER);

      previousMillis = now;
      patternStep = 1;
      break;

    case 1:

      if (now - previousMillis >= THUNDER_BURST_1) {

        motorOff();

        previousMillis = now;
        patternStep = 2;
      }
      break;

    case 2:

      if (now - previousMillis >= THUNDER_GAP_1) {

        setMotor(THUNDER_POWER);

        previousMillis = now;
        patternStep = 3;
      }
      break;

    case 3:

      if (now - previousMillis >= THUNDER_BURST_2) {

        motorOff();

        previousMillis = now;
        patternStep = 4;
      }
      break;

    case 4:

      if (now - previousMillis >= THUNDER_GAP_2) {

        setMotor(THUNDER_POWER);

        previousMillis = now;
        patternStep = 5;
      }
      break;

    case 5:

      if (now - previousMillis >= THUNDER_BURST_3) {

        motorOff();

        previousMillis = now;
        patternStep = 6;
      }
      break;

    case 6:

      if (now - previousMillis >= THUNDER_PAUSE) {

        patternStep = 0;
      }
      break;
  }
}

// ==================================================
// 13. HTTP Command Handler
// W / R / T / S
// ==================================================

void handleCommand() {

  if (!server.hasArg("mode")) {

    server.send(
      400,
      "text/plain",
      "Missing mode"
    );

    return;
  }

  String command = server.arg("mode");

  command.toUpperCase();

  if (command == "W") {
    setMode(WIND);
  }

  else if (command == "R") {
    setMode(RAIN);
  }

  else if (command == "T") {
    setMode(THUNDER);
  }

  else if (command == "S") {
    setMode(STOP);
  }

  else {

    server.send(
      400,
      "text/plain",
      "Invalid command"
    );

    return;
  }

  server.send(
    200,
    "text/plain",
    "OK: " + command
  );
}

// ==================================================
// 14. Heartbeat Handler
// ==================================================

void handleHeartbeat() {

  lastHeartbeat = millis();

  server.send(
    200,
    "text/plain",
    "ALIVE"
  );
}

// ==================================================
// 15. Status Handler
// ==================================================

String getModeName() {

  switch (currentMode) {

    case WIND:
      return "WIND";

    case RAIN:
      return "RAIN";

    case THUNDER:
      return "THUNDER";

    default:
      return "STOP";
  }
}

void handleStatus() {

  String json = "{";

  json += "\"mode\":\"";
  json += getModeName();
  json += "\",";

  json += "\"vibrating\":";
  json += (currentMode != STOP ? "true" : "false");

  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// ==================================================
// 16. Web Interface
// ==================================================

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport"
        content="width=device-width, initial-scale=1">

  <title>Museum Haptic Controller</title>

  <style>
    body {
      font-family: Arial, sans-serif;
      background: #15191f;
      color: white;
      text-align: center;
      padding: 30px;
    }

    button {
      display: block;
      width: 90%;
      max-width: 300px;
      margin: 15px auto;
      padding: 18px;
      font-size: 20px;
      border: none;
      border-radius: 12px;
      cursor: pointer;
    }

    .wind { background: #8ac7ff; }
    .rain { background: #5d9cec; }
    .thunder { background: #ffb347; }
    .stop { background: #ff6666; }

    #status {
      margin-top: 25px;
      font-size: 18px;
    }
  </style>
</head>

<body>

  <h2>Dragon Claw Haptic Controller</h2>

  <p>Select a weather mode</p>

  <button class="wind" onclick="send('W')">
    Wind
  </button>

  <button class="rain" onclick="send('R')">
    Rain
  </button>

  <button class="thunder" onclick="send('T')">
    Thunder
  </button>

  <button class="stop" onclick="send('S')">
    Stop
  </button>

  <p id="status">Current Mode: STOP</p>

  <script>
    let heartbeatTimer = null;

    async function send(mode) {
      try {
        const r = await fetch('/command?mode=' + mode);
        if (!r.ok) throw new Error('Command failed');

        if (heartbeatTimer) {
          clearInterval(heartbeatTimer);
          heartbeatTimer = null;
        }

        if (mode !== 'S') {
          heartbeatTimer = setInterval(async () => {
            try {
              await fetch('/heartbeat');
            } catch (e) {
              document.getElementById('status').innerText =
                'Connection lost';
            }
          }, 2000);
        }

        document.getElementById('status').innerText =
          'Command: ' + mode;

      } catch (e) {
        document.getElementById('status').innerText =
          'Connection error';
      }
    }
  </script>

</body>
</html>
)HTML";

void handleRoot() {

  server.send_P(
    200,
    "text/html; charset=utf-8",
    INDEX_HTML
  );
}

// ==================================================
// 17. Setup
// ==================================================

void setup() {

  Serial.begin(115200);

  pinMode(MOTOR_PIN, OUTPUT);

  motorOff();

  WiFi.mode(WIFI_AP);

  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", handleRoot);
  server.on("/command", handleCommand);
  server.on("/heartbeat", handleHeartbeat);
  server.on("/status", handleStatus);

  server.begin();

  lastHeartbeat = millis();

  Serial.println("============================");
  Serial.println("XLOT-Hermes Ready");
  Serial.println("============================");

  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());
}

// ==================================================
// 18. Main Loop
// ==================================================

void loop() {

  server.handleClient();

  // Safety watchdog
  if (
    currentMode != STOP &&
    millis() - lastHeartbeat > COMMAND_TIMEOUT
  ) {

    Serial.println("Heartbeat timeout -> STOP");

    setMode(STOP);
  }

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
      break;
  }

  delay(5);
}