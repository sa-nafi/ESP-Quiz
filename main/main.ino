#include <LittleFS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "secrets.h"

// Initialize the LCD display (I2C address 0x27, 16 columns, 2 rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ==========================================
// HARDWARE PIN DEFINITIONS
// ==========================================
#define P1_PIN 34
#define P2_PIN 35
#define P3_PIN 32
#define P4_PIN 33
#define RESET_PIN 25
#define GAME_RESET_PIN 26

// ==========================================
// WIFI & SERVER SETUP
// ==========================================
const char* ssid = SSID;
const char* password = PASS;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ==========================================
// QUIZ LOGIC & STATE MANAGEMENT
// ==========================================
enum GameState {
  NOT_STARTED = 0,
  WAITING_FOR_BUZZ = 1,
  ANSWERING = 2,
  ROUND_OVER = 3,
  GAME_OVER = 4
};

GameState currentState = NOT_STARTED;

struct Question {
  String text;
  String options[4];
  int correctOption; // 0 to 3 max
};

// 10 Sample Questions (MCQ format)
Question quiz[10] = {
  {"What is the capital of France?", {"London", "Berlin", "Paris", "Madrid"}, 2},
  {"What is 8 + 5?", {"11", "12", "13", "14"}, 2},
  {"Which planet is known as the Red Planet?", {"Earth", "Mars", "Jupiter", "Venus"}, 1},
  {"What is the chemical symbol for water?", {"H2O", "CO2", "HO2", "OH"}, 0},
  {"Who wrote Romeo and Juliet?", {"Charles Dickens", "William Shakespeare", "Mark Twain", "Jane Austen"}, 1},
  {"How many continents are there?", {"5", "6", "7", "8"}, 2},
  {"What is the largest mammal in the world?", {"Elephant", "Blue Whale", "Giraffe", "Shark"}, 1},
  {"In what year did the Titanic sink?", {"1910", "1912", "1914", "1916"}, 1},
  {"What is the boiling point of water in Celsius?", {"90", "100", "110", "120"}, 1},
  {"What is the hardest natural substance on Earth?", {"Gold", "Iron", "Diamond", "Platinum"}, 2}
};

const int totalQuestions = 10;
int currentQuestionIdx = 0;
int activePlayer = -1;
int scores[4] = {0, 0, 0, 0};
String lastResult = "";

// ==========================================
// HELPER FUNCTIONS
// ==========================================

// Pulses the hardware reset pin for the CD4013 latches
void resetBuzzer() {
  digitalWrite(RESET_PIN, HIGH);
  delay(100);
  digitalWrite(RESET_PIN, LOW);
}

// Packages the current game state into JSON and broadcasts to all connected WebSockets
void broadcastState() {
  StaticJsonDocument<1024> doc;
  doc["state"] = currentState;
  doc["qIdx"] = currentQuestionIdx;
  if (currentQuestionIdx < totalQuestions) {
    doc["qText"] = quiz[currentQuestionIdx].text;
    
    JsonArray jsonOpts = doc.createNestedArray("qOptions");
    for (int i = 0; i < 4; i++) {
      jsonOpts.add(quiz[currentQuestionIdx].options[i]);
    }
  } else {
    doc["qText"] = "Game Over!";
    doc.createNestedArray("qOptions");
  }
  
  doc["activePlayer"] = activePlayer;
  doc["resultMsg"] = lastResult;
  doc["isLast"] = (currentQuestionIdx >= totalQuestions - 1);
  
  JsonArray jsonScores = doc.createNestedArray("scores");
  for (int i = 0; i < 4; i++) {
    jsonScores.add(scores[i]);
  }

  String output;
  serializeJson(doc, output);
  ws.textAll(output);
}

// ==========================================
// WEBSOCKET HANDLER
// ==========================================
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, (const char*)data, len);
    if (err) return;

    String action = doc["action"];

    if (action == "start") {
      // Reset game parameters
      for(int i=0; i<4; i++) scores[i] = 0;
      currentQuestionIdx = 0;
      activePlayer = -1;
      lastResult = "";
      resetBuzzer();
      currentState = WAITING_FOR_BUZZ;
      broadcastState();
    } 
    else if (action == "reset") {
      // Reset game to not started
      for(int i=0; i<4; i++) scores[i] = 0;
      currentQuestionIdx = 0;
      activePlayer = -1;
      lastResult = "";
      resetBuzzer();
      currentState = NOT_STARTED;
      broadcastState();
    }
    else if (action == "submit" && currentState == ANSWERING) {
      int submittedOption = doc["answer"];
      int correctOption = quiz[currentQuestionIdx].correctOption;
      String correctAnswerStr = quiz[currentQuestionIdx].options[correctOption];

      if (submittedOption == correctOption) {
        scores[activePlayer - 1]++;
        lastResult = "<span style='color:green;'>Correct! The answer was " + correctAnswerStr + "</span>";
      } else {
        lastResult = "<span style='color:red;'>Incorrect. The correct answer was " + correctAnswerStr + "</span>";
      }
      
      currentState = ROUND_OVER;
      broadcastState();
    }
    else if (action == "next" && currentState == ROUND_OVER) {
      currentQuestionIdx++;
      if (currentQuestionIdx >= totalQuestions) {
        currentState = GAME_OVER;
      } else {
        activePlayer = -1;
        lastResult = "";
        resetBuzzer();
        currentState = WAITING_FOR_BUZZ;
      }
      broadcastState();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client #%u connected\n", client->id());
    broadcastState(); // Send current state to newly connected client
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    handleWebSocketMessage(arg, data, len);
  }
}

// ==========================================
// ARDUINO SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000); // Give Serial monitor time to initialize

  // Explicitly start I2C (SDA=21, SCL=22 for most ESP32s) to prevent pin conflicts
  Wire.begin();

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("WiFi: Disconnect");

  // Initialize Pins
  pinMode(P1_PIN, INPUT); // CD4013 provides hard HIGH/LOW, no pullup needed
  pinMode(P2_PIN, INPUT);
  pinMode(P3_PIN, INPUT);
  pinMode(P4_PIN, INPUT);
  
  pinMode(RESET_PIN, OUTPUT);
  pinMode(GAME_RESET_PIN, INPUT_PULLUP);
  resetBuzzer(); // Clear any phantom latches on boot

  if(!LittleFS.begin(true)){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP Address: ");
  Serial.println(WiFi.localIP());

  // Update LCD with connected status and IP
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi: Connected");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());

  // Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/index.html", "text/html");
  });
  
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/style.css", "text/css");
  });
  
  server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(LittleFS, "/script.js", "application/javascript");
  });

  // Attach WebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Start Server
  server.begin();
  Serial.println("HTTP Server Started");
}

// ==========================================
// ARDUINO MAIN LOOP
// ==========================================
void loop() {
  // Keep WebSocket clients clean
  ws.cleanupClients();

  // Check physical game reset button (Active LOW due to INPUT_PULLUP)
  if (digitalRead(GAME_RESET_PIN) == LOW) {
    delay(50); // Simple debounce
    if (digitalRead(GAME_RESET_PIN) == LOW) {
      Serial.println("Physical reset triggered!");
      for(int i=0; i<4; i++) scores[i] = 0;
      currentQuestionIdx = 0;
      activePlayer = -1;
      lastResult = "";
      resetBuzzer();
      currentState = NOT_STARTED;
      broadcastState();
      while(digitalRead(GAME_RESET_PIN) == LOW) { delay(10); } // Wait for release
    }
  }

  // Hardware Polling ONLY when waiting for a buzz
  if (currentState == WAITING_FOR_BUZZ) {
    
    // Check which pin went high. 
    // Because the hardware latches and locks out others, only one should be high.
    if (digitalRead(P1_PIN) == HIGH) activePlayer = 1;
    else if (digitalRead(P2_PIN) == HIGH) activePlayer = 2;
    else if (digitalRead(P3_PIN) == HIGH) activePlayer = 3;
    else if (digitalRead(P4_PIN) == HIGH) activePlayer = 4;

    if (Serial.available() > 0) {
      char incomingByte = Serial.read();
      
      if (incomingByte == '1') activePlayer = 1;
      else if (incomingByte == '2') activePlayer = 2;
      else if (incomingByte == '3') activePlayer = 3;
      else if (incomingByte == '4') activePlayer = 4;
    }

    // If a player buzzed, update state and lock out further reads
    if (activePlayer != -1) {
      Serial.printf("Player %d buzzed in!\n", activePlayer);
      currentState = ANSWERING;
      broadcastState(); // Update UI instantly
    }
  }
}
