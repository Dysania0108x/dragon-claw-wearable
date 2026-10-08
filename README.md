# Dragon Claw Challenge

Dragon Claw Challenge is an interactive museum prototype developed for
CDE5312 in collaboration with ArCH Square Museum.

The project uses hand gesture recognition, visual and audio feedback,
and wearable vibration feedback to help visitors explore the cultural
meanings of dragon motifs on Chinese ceramics.

---

## 1. System Overview

The system uses a camera to capture the visitor's hand gestures.

The camera data is processed by a Raspberry Pi 5 using MediaPipe.
Recognised gestures are converted into integer parameters from 0 to 5.

These parameters are then used to trigger:

- Visual content on a TV or projector
- Sound effects
- Weather-related vibration feedback through an ESP32-S3 wearable

### System Flow

User Gesture
→ Camera Module
→ Raspberry Pi 5
→ MediaPipe Gesture Recognition
→ Gesture Parameter (0–5)
→ Visual / Audio / Haptic Output

For vibration feedback:

Raspberry Pi 5
→ Bluetooth Low Energy (BLE)
→ ESP32-S3
→ Vibration Motor

---

## 2. Gesture Mapping

| Parameter | Gesture | Interaction |
|-----------|---------|-------------|
| 0 | Fist | Clouds & Wind |
| 1 | Wrist Move | Thunder |
| 2 | Open Palm | Water & Rain |
| 3 | Three Fingers | Three-Claw |
| 4 | Four Fingers | Four-Claw |
| 5 | Five Fingers | Five-Claw |

The parameters 0–5 represent gesture recognition results.

They do NOT represent vibration intensity.

---

## 3. Output Mapping

### 0 — Fist: Clouds & Wind

Visual:
- Clouds / wind animation

Audio:
- Wind sound

Haptic:
- Gentle vibration

---

### 1 — Wrist Move: Thunder

Visual:
- Thunder / lightning animation

Audio:
- Thunder sound

Haptic:
- Short vibration burst

---

### 2 — Open Palm: Water & Rain

Visual:
- Water / rain animation

Audio:
- Rain sound

Haptic:
- Vibration pulses

---

### 3 — Three Fingers

Visual:
- Three-claw animation / information

Haptic:
- None

---

### 4 — Four Fingers

Visual:
- Four-claw animation / information

Haptic:
- None

---

### 5 — Five Fingers

Visual:
- Five-claw animation / information

Haptic:
- None

---

## 4. Hardware

### Main Processing

- Raspberry Pi 5
- Raspberry Pi Camera Module
- TV / projector
- Speaker

### Wearable Haptic System

- ESP32-S3 N16R8
- Vibration motor module
- Wearable enclosure / wrist attachment

The ESP32-S3 receives BLE commands from the Raspberry Pi and controls
the vibration motor.

---

## 5. Software

### Raspberry Pi

- Python
- MediaPipe
- OpenCV
- Bleak
- Media playback libraries

### ESP32-S3

- Arduino IDE
- ESP32 BLE
- GPIO vibration motor control

---

## 6. Project Structure

wearable/
│
├── media/
│   ├── wind.mp4
│   ├── wind.mp3
│   ├── rain.mp4
│   ├── rain.mp3
│   ├── thunder.mp4
│   ├── thunder.mp3
│   ├── claw_3.mp4
│   ├── claw_4.mp4
│   └── claw_5.mp4
│
├── ble_test.py
├── ble_control.py
├── gesture_detection.py
├── media_controller.py
├── vibration_controller.py
├── main.py
├── .gitignore
└── README.md

---

## 7. Python Modules

### `gesture_detection.py`

Responsible for:

- Receiving the camera feed
- Detecting hand landmarks using MediaPipe
- Recognising predefined gestures
- Returning a gesture parameter from 0 to 5

---

### `media_controller.py`

Responsible for:

- Mapping gesture parameters to visual content
- Playing MP4 animations
- Displaying images
- Playing corresponding sound effects
- Sending visual output to the TV / projector

---

### `vibration_controller.py`

Responsible for:

- Connecting the Raspberry Pi to the ESP32-S3 via BLE
- Sending weather-related vibration commands
- Triggering different haptic patterns for wind, rain, and thunder

Vibration is only used for weather-related interactions.

---

### `main.py`

Main system controller.

It connects:

Gesture Recognition
→ Gesture Parameter
→ Visual / Audio Output
→ Weather-related Vibration Output

This is the main program that will be executed on the Raspberry Pi.

---

### `ble_test.py`

Development / debugging tool.

Used to scan for the ESP32-S3 BLE device and confirm that it is
discoverable.

---

### `ble_control.py`

Development / debugging tool.

Used to manually send BLE commands to the ESP32-S3 before integrating
BLE control into the complete system.

---

## 8. Current Development Status

Completed:

- ESP32-S3 setup
- Vibration motor GPIO control
- ESP32-S3 BLE setup
- Python BLE device discovery
- Python → BLE → ESP32-S3 communication
- Python-controlled vibration test

In Development:

- Raspberry Pi camera setup
- MediaPipe gesture recognition
- Gesture classification (0–5)
- Visual / audio media triggering
- Weather vibration patterns
- Full system integration

---

## 9. Final Interaction Pipeline

Camera
→ Raspberry Pi 5
→ MediaPipe
→ Gesture Recognition
→ Parameter 0–5

Then:

Parameter
→ Visual / Audio Content
→ TV / Projector

and, for weather interactions:

Parameter 0 / 1 / 2
→ BLE
→ ESP32-S3
→ Vibration Motor

---

## 10. Prototype Goal

The prototype aims to connect visitors' physical hand gestures with
the visual features and cultural meanings of dragon motifs.

Three-, four-, and five-finger gestures support exploration of dragon
claw configurations, while wind, rain, and thunder interactions use
visual, audio, and haptic feedback to communicate dragons'
associations with natural forces.