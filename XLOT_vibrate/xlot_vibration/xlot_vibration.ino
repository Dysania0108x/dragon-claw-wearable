#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ==================================================
// 1. Hardware Configuration
// ==================================================

#define MOTOR_PIN 17

// ==================================================
// 2. Wi-Fi Configuration
// ==================================================

const char* WIFI_SSID = "hxever";
const char* WIFI_PASS = "hank_hsu";

const unsigned long WIFI_TIMEOUT = 10000;
const unsigned long WIFI_RETRY_INTERVAL = 10000;

unsigned long lastWiFiAttempt = 0;

bool mdnsStarted = false;

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
// 4. Vibration Intensity (0-100%)
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
  850,  // Light Rain
  450,  // Moderate Rain
  200   // Heavy Rain
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
// 8. Reset Pattern
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

  lastHeartbeat = millis();

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
// 10. WIND Pattern
// Gradual increase -> decrease -> pause
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
// 11. RAIN Pattern
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
// 12. THUNDER Pattern
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
// 13. Wi-Fi Connection
// ==================================================

bool connectWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println();
  Serial.println("Connecting to phone hotspot...");

  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);

  WiFi.disconnect();
  delay(100);

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startTime = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startTime < WIFI_TIMEOUT
  ) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println();
    Serial.println("Wi-Fi connected!");

    Serial.print("Connected SSID: ");
    Serial.println(WiFi.SSID());

    Serial.print("XLOT IP: ");
    Serial.println(WiFi.localIP());

    return true;
  }

  Serial.println();
  Serial.println("Wi-Fi connection failed.");

  return false;
}

// ==================================================
// 14. mDNS
// ==================================================

void startMDNS() {

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mdnsStarted) {
    return;
  }

  if (MDNS.begin("xlot-hermes")) {

    MDNS.addService("http", "tcp", 80);

    mdnsStarted = true;

    Serial.println("mDNS ready:");
    Serial.println("http://xlot-hermes.local");

  } else {

    Serial.println("mDNS initialization failed.");
  }
}

// ==================================================
// 15. HTTP Command Handler
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
// 16. Heartbeat Handler
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
// 17. Status Handler
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
// 18. Web Interface
// ==================================================

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">

<head>
  <meta name="viewport"
        content="width=device-width, initial-scale=1">

  <title>Dragon Claw Haptic Controller</title>

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

        if (!r.ok) {
          throw new Error('Command failed');
        }

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
// 19. Setup
// ==================================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  pinMode(MOTOR_PIN, OUTPUT);
  motorOff();

  Serial.println();
  Serial.println("============================");
  Serial.println("XLOT-Hermes Starting...");
  Serial.println("============================");

  // Wi-Fi Station Mode
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  connectWiFi();
  startMDNS();

  lastWiFiAttempt = millis();

  // HTTP Routes
  server.on("/", handleRoot);
  server.on("/command", handleCommand);
  server.on("/heartbeat", handleHeartbeat);
  server.on("/status", handleStatus);

  server.begin();

  lastHeartbeat = millis();

  Serial.println();
  Serial.println("HTTP Server Started");

  Serial.println("W = Wind");
  Serial.println("R = Rain");
  Serial.println("T = Thunder");
  Serial.println("S = Stop");

  Serial.println();
}

// ==================================================
// 20. Main Loop
// ==================================================

void loop() {

  // Wi-Fi reconnect
  if (WiFi.status() != WL_CONNECTED) {

    if (currentMode != STOP) {
      setMode(STOP);
    }

    if (mdnsStarted) {
      MDNS.end();
      mdnsStarted = false;
    }

    if (
      millis() - lastWiFiAttempt >= WIFI_RETRY_INTERVAL
    ) {

      lastWiFiAttempt = millis();

      if (connectWiFi()) {
        startMDNS();
      }
    }
  }

  // Handle HTTP requests
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  // Heartbeat timeout
  if (
    currentMode != STOP &&
    millis() - lastHeartbeat > COMMAND_TIMEOUT
  ) {

    Serial.println("Heartbeat timeout -> STOP");

    setMode(STOP);
  }

  // Update vibration pattern
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