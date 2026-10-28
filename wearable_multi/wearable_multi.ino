#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <XLOT_PWMServoDriver.h>

// Dragon Claw 4-motor firmware (XLOT-Hermes ESP32-S2 + PCA9685)
// Configure locally before flashing. Never commit real hotspot credentials.
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
constexpr uint8_t SDA_PIN=17, SCL_PIN=18, PCA_ADDR=0x40, MOTOR_COUNT=4;
constexpr uint32_t HEARTBEAT_TIMEOUT=6000, TEST_DURATION=1000;
const uint8_t MOTOR_CHANNELS[4]={6,7,8,9};
const uint8_t TEST_POWER=30, WIND_POWER=35, RAIN_POWER=40, THUNDER_POWER=50;
WebServer server(80);
XLOT_PWMServoDriver pwm(PCA_ADDR);
enum Mode { MODE_STOP, MODE_TEST, MODE_WIND, MODE_RAIN, MODE_THUNDER };
Mode currentMode=MODE_STOP;
bool pcaReady=false;
uint8_t motorPower[4]={0,0,0,0};
uint32_t modeStart=0, lastHeartbeat=0, lastWifiRetry=0;
// Rain: independent pulse expiry and randomly scheduled drops.
uint32_t nextRainDrop=0;
uint32_t rainOffAt[MOTOR_COUNT]={0,0,0,0};
int lastRainMotor=-1;
void writeMotor(uint8_t index,uint8_t power){
  if(!pcaReady || index>=MOTOR_COUNT)return;
  power=constrain(power,0,100);
  if(motorPower[index]==power)return;
  pwm.setPWM(MOTOR_CHANNELS[index],0,((uint32_t)power*4095UL)/100UL);
  motorPower[index]=power;
}
void allMotorsOff(){for(uint8_t i=0;i<MOTOR_COUNT;i++)writeMotor(i,0);}
void setAllMotors(uint8_t p){for(uint8_t i=0;i<MOTOR_COUNT;i++)writeMotor(i,p);}
void stopMotors(){allMotorsOff();currentMode=MODE_STOP;Serial.println("Mode -> STOP");}
void startMode(Mode m){
  if(!pcaReady)return;
  stopMotors();
  currentMode=m;
  modeStart=millis();
  lastHeartbeat=millis();
  if(m==MODE_RAIN){
    nextRainDrop=millis();
    lastRainMotor=-1;
    for(uint8_t i=0;i<MOTOR_COUNT;i++)rainOffAt[i]=0;
  }
}
void startTest(int i){if(!pcaReady||i<0||i>=MOTOR_COUNT)return;startMode(MODE_TEST);writeMotor(i,TEST_POWER);Serial.printf("Testing Motor %d\n",i+1);}
// Wind: each motor stays on 450 ms, next starts every 300 ms.
// 150 ms overlap, including M4 -> M1 across cycle boundary.
void updateWind(){
  const uint32_t STEP_MS=300, PULSE_MS=450;
  const uint32_t CYCLE_MS=STEP_MS*MOTOR_COUNT;
  uint32_t phase=(millis()-modeStart)%CYCLE_MS;
  for(uint8_t i=0;i<MOTOR_COUNT;i++){
    uint32_t elapsed=(phase+CYCLE_MS-i*STEP_MS)%CYCLE_MS;
    writeMotor(i,elapsed<PULSE_MS?WIND_POWER:0);
  }
}
// Rain: non-repeating random raindrops, 60-150 ms each,
// 80-250 ms between starts; max two motors active at a time.
void updateRain(){
  uint32_t now=millis();
  for(uint8_t i=0;i<MOTOR_COUNT;i++){
    if(rainOffAt[i]!=0 && (int32_t)(now-rainOffAt[i])>=0){
      writeMotor(i,0);
      rainOffAt[i]=0;
    }
  }
  if((int32_t)(now-nextRainDrop)<0)return;
  uint8_t selected=random(MOTOR_COUNT);
  if(selected==lastRainMotor)selected=(selected+random(1,MOTOR_COUNT))%MOTOR_COUNT;
  // Never cut off an existing drop; skip if chosen channel is busy.
  if(rainOffAt[selected]==0){
    uint32_t duration=random(60,151);
    uint8_t power=random(30,46);
    writeMotor(selected,power);
    rainOffAt[selected]=now+duration;
    lastRainMotor=selected;
  }
  nextRainDrop=now+random(80,251);
}
void updateThunder(){uint32_t t=(millis()-modeStart)%2400;if(t<180||(t>=420&&t<700))setAllMotors(THUNDER_POWER);else allMotorsOff();}
void initializePCA(){
  Serial.println("Initializing I2C...");
  bool ok=Wire.begin(SDA_PIN,SCL_PIN);
  if(!ok){Serial.println("I2C initialization failed");return;}
  Wire.setClock(100000);Wire.setTimeOut(100);
  Wire.beginTransmission(PCA_ADDR);
  uint8_t result=Wire.endTransmission();
  Serial.printf("PCA9685 probe result: %u\n",result);
  if(result!=0){Serial.println("PCA9685 NOT detected; motor commands disabled");return;}
  pwm.begin();pwm.setOscillatorFrequency(27000000UL);pwm.setPWMFreq(1000);
  for(uint8_t ch=0;ch<16;ch++)pwm.setPWM(ch,0,0);
  pcaReady=true;
  Serial.println("PCA9685 detected and initialized!");
}
const char PAGE[] PROGMEM=R"HTML(
<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>XLOT 4-Motor Controller</title>
<style>body{font:16px Arial,sans-serif;background:#101827;color:#fff;max-width:640px;margin:auto;padding:24px}button{background:#3478d4;color:white;border:0;border-radius:9px;padding:14px;margin:5px}button.stop{background:#c9424b}pre{background:#202d43;padding:16px;border-radius:9px;white-space:pre-wrap}</style></head>
<body><h1>XLOT 4-Motor Controller</h1><h3>Individual Motor Test</h3><button onclick="testMotor(0)">M1</button><button onclick="testMotor(1)">M2</button><button onclick="testMotor(2)">M3</button><button onclick="testMotor(3)">M4</button><h3>Weather Patterns</h3><button onclick="sendMode('W')">Wind</button><button onclick="sendMode('R')">Rain</button><button onclick="sendMode('T')">Thunder</button><button class="stop" onclick="sendMode('S')">STOP</button><h3>Device Status</h3><button onclick="checkStatus()">Check Status</button><pre id="status">Connecting...</pre>
<script>
let timer=null;
function clearHeartbeat(){if(timer!==null){clearInterval(timer);timer=null;}}
async function request(path){const r=await fetch(path),t=await r.text();document.getElementById('status').textContent=t;if(!r.ok)throw Error(t);return t;}
async function testMotor(i){clearHeartbeat();try{await request('/test?ch='+i)}catch(e){console.error(e)}}
async function sendMode(m){clearHeartbeat();try{await request('/command?mode='+m);if(m!=='S')timer=setInterval(()=>fetch('/heartbeat').catch(console.error),2000)}catch(e){console.error(e)}}
async function checkStatus(){try{await request('/status')}catch(e){console.error(e)}}
window.addEventListener('pagehide',()=>{clearHeartbeat();navigator.sendBeacon('/stop')});checkStatus();
</script></body></html>
)HTML";
void handleTest(){
  if(!pcaReady){server.send(503,"text/plain","PCA9685 not detected");return;}
  if(!server.hasArg("ch")){server.send(400,"text/plain","Missing channel");return;}
  String a=server.arg("ch");
  if(a.length()!=1||a[0]<'0'||a[0]>'3'){server.send(400,"text/plain","Invalid channel");return;}
  startTest(a.toInt());server.send(200,"text/plain","Motor test started");
}
void handleCommand(){
  String m=server.arg("mode");m.toUpperCase();
  if(m=="S"){stopMotors();server.send(200,"text/plain","STOP");return;}
  if(!pcaReady){server.send(503,"text/plain","PCA9685 not detected");return;}
  if(m=="W")startMode(MODE_WIND);
  else if(m=="R")startMode(MODE_RAIN);
  else if(m=="T")startMode(MODE_THUNDER);
  else{server.send(400,"text/plain","Invalid mode");return;}
  server.send(200,"text/plain","OK: "+m);
}
void handleStatus(){
  const char* names[]={"STOP","TEST","WIND","RAIN","THUNDER"};
  String j="{\"ready\":";j+=(pcaReady?"true":"false");
  j+=",\"mode\":\"";j+=names[(int)currentMode];j+="\",\"vibrating\":";
  j+=(currentMode!=MODE_STOP?"true":"false");j+=",\"motors\":[";
  for(int i=0;i<4;i++){if(i)j+=",";j+=String(motorPower[i]);}j+="]}";
  server.send(200,"application/json",j);
}
void connectWiFi(){
  WiFi.mode(WIFI_STA);WiFi.setSleep(false);WiFi.begin(WIFI_SSID,WIFI_PASS);
  Serial.print("Connecting to Wi-Fi: ");Serial.println(WIFI_SSID);
  uint32_t started=millis();
  while(WiFi.status()!=WL_CONNECTED&&millis()-started<15000){delay(500);Serial.print(".");}
  Serial.println();
  if(WiFi.status()==WL_CONNECTED){Serial.print("Wi-Fi connected! IP: ");Serial.println(WiFi.localIP());}
  else Serial.println("Wi-Fi connection pending.");
}
void setup(){
  Serial.begin(115200);delay(1200);
  randomSeed(esp_random());
  Serial.println("Dragon Claw 4-Motor V4.1 - Overlap Wind / Random Rain");
  initializePCA();connectWiFi();
  server.on("/",[](){server.send_P(200,"text/html; charset=utf-8",PAGE);});
  server.on("/test",handleTest);server.on("/command",handleCommand);
  server.on("/heartbeat",[](){if(pcaReady&&currentMode!=MODE_STOP&&currentMode!=MODE_TEST)lastHeartbeat=millis();server.send(200,"text/plain","ALIVE");});
  server.on("/status",handleStatus);server.on("/stop",[](){stopMotors();server.send(200,"text/plain","STOP");});
  server.begin();Serial.println("HTTP Server Started");
}
void loop(){
  server.handleClient();
  if(WiFi.status()!=WL_CONNECTED){
    if(currentMode!=MODE_STOP)stopMotors();
    if(millis()-lastWifiRetry>15000){lastWifiRetry=millis();WiFi.disconnect();WiFi.begin(WIFI_SSID,WIFI_PASS);Serial.println("Wi-Fi reconnecting...");}
  }
  if((currentMode==MODE_WIND||currentMode==MODE_RAIN||currentMode==MODE_THUNDER)&&millis()-lastHeartbeat>HEARTBEAT_TIMEOUT){Serial.println("Heartbeat timeout");stopMotors();}
  if(pcaReady){
    switch(currentMode){
      case MODE_TEST:if(millis()-modeStart>=TEST_DURATION)stopMotors();break;
      case MODE_WIND:updateWind();break;
      case MODE_RAIN:updateRain();break;
      case MODE_THUNDER:updateThunder();break;
      default:break;
    }
  }
  delay(5);
}
