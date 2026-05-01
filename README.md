# ESP Quiz

## Overview
The ESP Quiz is an interactive, real-time quiz game platform built on the Arduino framework. It combines custom hardware logic (74HC08 and CD4013 ICs) for zero-latency, first-press buzzer detection with a responsive web frontend served directly from the ESP32. Bidirectional communication between the hardware and the user interface is handled via WebSockets, ensuring seamless state synchronization without page reloads.

## Features
- **Hardware-Level Latching**: Utilizes external logic gates for immediate buzzer detection without the need for software debouncing.
- **Real-Time Web Interface**: Embedded HTML/JS/CSS frontend served from LittleFS using `ESPAsyncWebServer`.
- **WebSocket Protocol**: Fast, bidirectional JSON messaging using `AsyncWebSocket` and `ArduinoJson`.
- **State Machine Architecture**: Robust, strict state management ensuring orderly game progression.
- **I2C LCD Integration**: Displays network connection status and the local IP address for easy discovery of the web portal.

## Hardware Configuration

### Pin Mapping
- **Player Inputs (Active HIGH from CD4013 latches)**:
  - Player 1: GPIO 34
  - Player 2: GPIO 35
  - Player 3: GPIO 32
  - Player 4: GPIO 33
- **Control Pins**:
  - Hardware Reset: GPIO 25 (Sends a 100ms HIGH pulse to reset external latches)
  - Game Reset: GPIO 26 (Physical button to reset the system state, Active LOW)
- **I2C LCD Display**:
  - SDA: GPIO 21
  - SCL: GPIO 22

## System Architecture

### Core State Machine (`GameState`)
The game flow is controlled by an explicit state machine. All logic and frontend updates must adhere to these states:
- `NOT_STARTED (0)`: Initial state. UI displays the "Start Game" button.
- `WAITING_FOR_BUZZ (1)`: ESP32 polls hardware inputs. UI hides active inputs.
- `ANSWERING (2)`: A player has buzzed in. Hardware polling is paused. UI presents the answer field to the active player.
- `ROUND_OVER (3)`: Answer submitted. UI displays the result and prompts for the next question.
- `GAME_OVER (4)`: All questions completed. UI shows final scores and prompts to restart.

### Communication Protocol
The system uses JSON over WebSockets (`/ws`) for all client-server communication.

**Server to Client Broadcasts:**
The ESP32 pushes game state updates whenever a transition occurs.
```json
{
  "state": 2,
  "qIdx": 0,
  "qText": "What is 8 + 5?",
  "qOptions": [
    "11",
    "12",
    "13",
    "14"
  ],
  "activePlayer": 1,
  "resultMsg": "...",
  "isLast": false,
  "scores": [0, 1, 0, 0]
}
```

**Client to Server Actions:**
The frontend sends action commands based on user interaction.
- Start/Restart: `{"action": "start"}`
- Submit Answer: `{"action": "submit", "answer": 2}`
- Next Question: `{"action": "next"}`
- Reset Game: `{"action": "reset"}`

## Modification and Contribution Guide

### Modifying the Frontend
The HTML, CSS, and JS files are stored in the LittleFS filesystem (located in the `data/` directory).
- To update the frontend, edit the respective files in `data/`.
- Ensure you upload the LittleFS filesystem image to the ESP32 after making changes to these files (e.g., using the ESP32 Sketch Data Upload tool).
- The `updateUI(data)` JavaScript function must be updated if new states or payload variables are introduced.

### Developing the Firmware
- **Non-blocking Code**: The system relies on asynchronous web servers and WebSockets. Never use `delay()` in the `loop()` or in WebSocket handlers. Use `millis()` for any time-based logic.
- **Hardware Abstraction**: Do not add software debouncing for the buzzers. The external CD4013 ICs handle this natively.
- **Quiz Data**: Questions are stored in a `Question` struct array in memory. If adding a large dataset, consider migrating this structure to LittleFS or PROGMEM to conserve RAM.
- **Display Additions**: The I2C LCD is initialized via the `LiquidCrystal_I2C` library. Changes to the display logic should occur in `setup()` or state transition blocks to avoid unnecessary I2C traffic in the main loop.

## Setup and Installation
1. Install the required Arduino libraries: `ESPAsyncWebServer`, `AsyncTCP`, `ArduinoJson`, and `LiquidCrystal_I2C`.
2. Configure your WiFi credentials (`ssid` and `password`) in `main/main.ino`.
3. Use the Arduino IDE to upload the LittleFS data folder to the ESP32 (using the [LittleFS Upload Tool](https://github.com/earlephilhower/arduino-littlefs-upload)).
4. Compile and upload the sketch to the ESP32.
5. Check the I2C LCD for the assigned IP address, and navigate to it in a web browser to start the game.
