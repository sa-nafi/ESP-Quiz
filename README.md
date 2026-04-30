***

# ESP32 Quiz Buzzer System - Context & Architecture Guide

## 1. System Overview
This project is an ESP32-based quiz buzzer system running the Arduino framework. It interfaces with a custom hardware circuit (using 74HC08 and CD4013 logic ICs) that acts as a physical first-press latch. The ESP32 serves an embedded HTML/JS/CSS frontend via `ESPAsyncWebServer` and handles real-time bidirectional communication via WebSockets (`AsyncWebSocket`).

## 2. Hardware Abstraction & Pin Mapping
The hardware logic gates handle debouncing and first-press latching. Therefore, the ESP32 software **does not** need software debouncing. It simply polls the pins, registers the first HIGH signal, and immediately locks state.

*   **Player 1 Input:** GPIO 34
*   **Player 2 Input:** GPIO 35
*   **Player 3 Input:** GPIO 32
*   **Player 4 Input:** GPIO 33
    *   *Logic:* Active HIGH (3.3V) from external CD4013 latches.
*   **Hardware Reset:** GPIO 25
    *   *Logic:* A 100ms HIGH pulse resets the external CD4013 latches. Triggered via the `resetBuzzer()` function.

## 3. Core State Machine (`GameState`)
The system is strictly governed by an `enum GameState`. **Any future AI modifying this code MUST adhere to these state transitions.**

*   `0 | NOT_STARTED`: Initial boot state. UI shows "Start Game" button.
*   `1 | WAITING_FOR_BUZZ`: ESP32 actively polls GPIOs 32-35 in `loop()`. UI hides inputs.
*   `2 | ANSWERING`: A player has buzzed. **Hardware polling stops.** UI shows answer input field to the active player.
*   `3 | ROUND_OVER`: Answer submitted. UI displays correct/incorrect result and "Next Question" button.
*   `4 | GAME_OVER`: All questions exhausted. UI shows final scores and "Play Again" button.

## 4. WebSocket Communication Protocol
Communication is entirely real-time JSON over WebSockets (port 80, path `/ws`). AJAX is **not** used. The `ArduinoJson` library handles serialization/deserialization.

### Server -> Client (State Broadcasts)
The ESP32 pushes the entire game state to the browser via `broadcastState()` whenever a state changes.
```json
{
  "state": 2,                 // Matches GameState enum
  "qIdx": 0,                  // Current question index
  "qText": "What is 8 + 5?",  // Current question string
  "activePlayer": 1,          // 1-4 (or -1 if none)
  "resultMsg": "...",         // HTML string of the last answer result
  "scores": [0, 1, 0, 0]      // Array of scores for P1, P2, P3, P4
}
```

### Client -> Server (Action Requests)
The browser sends actions to the ESP32 based on button clicks.
*   **Start/Restart:** `{"action": "start"}`
*   **Submit Answer:** `{"action": "submit", "answer": "User Typed String"}`
*   **Next Question:** `{"action": "next"}`

## 5. File Structure & Memory Management
*   **Frontend Storage:** The HTML, CSS, and JS are bundled into a single `PROGMEM` string (`index_html`). This avoids the need for SPIFFS/LittleFS uploads, keeping deployment to a single `.ino` upload.
*   **String Handling:** Answers are checked case-insensitively using `String.toLowerCase()` and `String.trim()`.

## 6. Guidelines for AI Agents Modifying This Code
If you are an AI tasked with updating this codebase, adhere to the following rules:

1.  **Do Not Block the Loop:** The system uses `ESPAsyncWebServer`. You must never use `delay()` in the `loop()` or in WebSocket handlers (except for the 100ms `resetBuzzer()` pulse, which is acceptable). Use `millis()` for any new timers.
2.  **Frontend Updates:** If modifying the UI, edit the `index_html` raw literal string. You must update the `updateUI(data)` JavaScript function if you add new states or variables.
3.  **Adding a Timer (Common Feature Request):**
    *   If tasked with adding a countdown timer, implement the countdown logic in the JavaScript frontend to save ESP32 processing power.
    *   Only use the ESP32 to enforce a timeout (e.g., storing a `millis()` timestamp when state enters `ANSWERING`).
4.  **Adding Audio (Common Feature Request):**
    *   If adding a buzzer sound, trigger it immediately inside the `WAITING_FOR_BUZZ` block in the `loop()` exactly when a GPIO goes HIGH, before calling `broadcastState()`. Use a non-blocking `tone()` or a simple digital HIGH to an active buzzer pin.
5.  **Question Data Structure:** Questions are stored in a simple `struct Question` array. If adding many questions, consider moving this to a separate `.h` file or formatting it in JSON within SPIFFS if memory limits are reached. Currently, it resides in RAM.
