#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// ==========================================
// ⚙️ HARDWARE PIN DEFINITIONS
// ==========================================
#define P1_PIN 34
#define P2_PIN 35
#define P3_PIN 32
#define P4_PIN 33
#define RESET_PIN 25
#define GAME_RESET_PIN 26

// ==========================================
// 🌐 WIFI & SERVER SETUP
// ==========================================
const char* ssid = "Patriot";
const char* password = "67932108";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ==========================================
// 🧠 QUIZ LOGIC & STATE MANAGEMENT
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
// 🎨 FRONTEND (HTML + CSS + JS)
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP Quiz</title>
<style>
@import url('https://fonts.googleapis.com/css2?family=Balsamiq+Sans:wght@400;700&display=swap');
body { background-color: #fdfcf0; background-image: radial-gradient(#d1d1d1 1px, transparent 1px); background-size: 20px 20px; font-family: 'Balsamiq Sans', 'Comic Sans MS', sans-serif; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; color: #000; font-size: 1.1rem; font-weight: 700; }
.container { width: 90%; max-width: 500px; text-align: center; padding: 20px 0; }
.sketch-box { background: #fff; border: 3px solid #000; margin-bottom: 20px; padding: 20px; border-radius: 255px 15px 225px 15px/15px 225px 15px 255px; box-shadow: 5px 5px 0px rgba(0,0,0,0.15); }
h1 { margin: 0; font-size: 2.2rem; text-shadow: 1px 1px 0 #fff; }
p { margin: 5px 0 0; font-style: italic; opacity: 1; font-size: 1.1rem; font-weight: bold; }
.status-msg { font-weight: bold; margin-bottom: 20px; font-size: 1.4rem; text-shadow: 1px 1px 0 #fff; }
.status-msg.waiting { color: #d9534f; }
.status-msg.buzzed { color: #047857; animation: blink 1s infinite alternate; }
.status-msg.over { color: #000; }
@keyframes blink { from { opacity: 1; } to { opacity: 0.6; } }
button { border: 3px solid #000; padding: 10px 20px; font-size: 1.2rem; font-family: inherit; cursor: pointer; border-radius: 10px 100px 15px 100px/100px 15px 100px 15px; transition: transform 0.1s; margin: 5px; font-weight: bold; color: #000; }
button:active { transform: scale(0.95) rotate(-2deg); }
.btn-start { background: #b8e994; }
.btn-next { background: #82ccdd; }
.btn-reset { background: #ffb8b8; }
#mcq-options { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 10px; }
.mcq-btn { width: 100%; background: #fff; margin: 0; display: block; font-size: 1.2rem; }
.mcq-btn:hover { background: #fdfcf0; }
.scoreboard { display: grid; grid-template-columns: repeat(4, 1fr); gap: 10px; margin-top: 15px; }
.score-card { padding: 10px 5px; border: 2px solid #000; border-radius: 50% 45% 52% 48% / 48% 52% 45% 50%; transition: 0.2s; background: #fff; }
.score-card.active { background: #b8e994; transform: scale(1.1) rotate(5deg); box-shadow: 3px 3px 0 rgba(0,0,0,0.3); }
.score-label { font-size: 1rem; display: block; font-weight: bold; }
.score-value { font-size: 1.8rem; font-weight: bold; color: #0984e3; text-shadow: 1px 1px 0 #fff; }
#q-container { font-size: 1.4rem; font-weight: bold; }
#q-text { font-size: 1.3rem; font-weight: bold; margin-top: 10px; display: block; color: #000; }
#result-msg { font-weight: bold; margin-top: 10px; font-size: 1.4rem; text-shadow: 1px 1px 0 #fff; }
.input-group { display: none; }
.action-box { background: none; border: none; box-shadow: none; padding: 0; }
</style>
</head>
<body>
<div class="container">
<div class="sketch-box">
<h1>ESP Quiz</h1>
</div>
<div id="q-container" class="sketch-box" style="display:none;">
Question <span id="q-num">1</span>:<br><span id="q-text">Loading...</span>
</div>
<div id="status-bar" class="status-msg waiting">Waiting to start...</div>
<div class="sketch-box action-box">
<button id="btn-start" class="btn-start" onclick="sendAction('start')">Start Game</button>
<button id="btn-next" class="btn-next" onclick="sendAction('next')" style="display:none;">Next Question</button>
<button id="btn-reset" class="btn-reset" onclick="confirmReset()" style="display:none;">Reset</button>
</div>
<div id="input-group" class="input-group sketch-box">
<div id="mcq-options">
<button class="mcq-btn" onclick="submitAnswer(0)" id="opt0">Option 1</button>
<button class="mcq-btn" onclick="submitAnswer(1)" id="opt1">Option 2</button>
<button class="mcq-btn" onclick="submitAnswer(2)" id="opt2">Option 3</button>
<button class="mcq-btn" onclick="submitAnswer(3)" id="opt3">Option 4</button>
</div>
<div id="result-msg"></div>
</div>
<div class="sketch-box">
<div style="font-size: 0.9rem; margin-bottom: 10px; text-decoration: underline;">SCOREBOARD</div>
<div class="scoreboard">
<div id="p1-card" class="score-card"><span class="score-label">P1</span><span id="s1" class="score-value">0</span></div>
<div id="p2-card" class="score-card"><span class="score-label">P2</span><span id="s2" class="score-value">0</span></div>
<div id="p3-card" class="score-card"><span class="score-label">P3</span><span id="s3" class="score-value">0</span></div>
<div id="p4-card" class="score-card"><span class="score-label">P4</span><span id="s4" class="score-value">0</span></div>
</div>
</div>
</div>
<script>
var gateway=`ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load',initWebSocket);
function initWebSocket(){
websocket=new WebSocket(gateway);
websocket.onopen=onOpen;
websocket.onclose=onClose;
websocket.onmessage=onMessage;
}
function onOpen(event){console.log('Connection opened');}
function onClose(event){setTimeout(initWebSocket,2000);}
function onMessage(event){
let data=JSON.parse(event.data);
updateUI(data);
}
function sendAction(actionStr){
websocket.send(JSON.stringify({action:actionStr}));
}
function confirmReset(){
if(confirm("Are you sure you want to reset the game? All scores will be lost.")){
sendAction('reset');
}
}
function submitAnswer(optIdx){
websocket.send(JSON.stringify({action:'submit',answer:optIdx}));
}
function updateUI(data){
for(let i=1;i<=4;i++){
document.getElementById('s'+i).innerText=data.scores[i-1];
document.getElementById('p'+i+'-card').classList.remove('active');
}
let state=data.state;
let statusDiv=document.getElementById('status-bar');
let qContainer=document.getElementById('q-container');
let inputGroup=document.getElementById('input-group');
let btnStart=document.getElementById('btn-start');
let btnNext=document.getElementById('btn-next');
let btnReset=document.getElementById('btn-reset');
let resultMsg=document.getElementById('result-msg');
if(state>0&&state<4){
qContainer.style.display="block";
document.getElementById('q-num').innerText=(data.qIdx+1);
document.getElementById('q-text').innerText=data.qText;
if(data.qOptions){
document.getElementById('opt0').innerText=data.qOptions[0];
document.getElementById('opt1').innerText=data.qOptions[1];
document.getElementById('opt2').innerText=data.qOptions[2];
document.getElementById('opt3').innerText=data.qOptions[3];
}
btnStart.style.display="none";
}
if(state===0){
statusDiv.className="status-msg waiting";
statusDiv.innerText="Press Start Game to begin!";
qContainer.style.display="none";
inputGroup.style.display="none";
btnNext.style.display="none";
btnStart.style.display="inline-block";
btnStart.innerText="Start Game";
btnReset.style.display="none";
}else if(state===1){
statusDiv.className="status-msg waiting";
statusDiv.innerText="Waiting for players to buzz...";
inputGroup.style.display="none";
btnNext.style.display="none";
resultMsg.innerText="";
btnReset.style.display="inline-block";
}else if(state===2){
statusDiv.className="status-msg buzzed";
statusDiv.innerText="Player "+data.activePlayer+" buzzed! Awaiting answer...";
document.getElementById('p'+data.activePlayer+'-card').classList.add('active');
inputGroup.style.display="block";
document.getElementById('mcq-options').style.display="grid";
btnNext.style.display="none";
btnReset.style.display="inline-block";
}else if(state===3){
statusDiv.className="status-msg over";
statusDiv.innerText="Round Over";
btnNext.style.display="inline-block";
if(data.isLast){
btnNext.innerText="Finish";
}else{
btnNext.innerText="Next Question";
}
resultMsg.innerHTML=data.resultMsg;
inputGroup.style.display="block";
document.getElementById('mcq-options').style.display="none";
btnReset.style.display="inline-block";
}else if(state===4){
statusDiv.className="status-msg over";
statusDiv.innerHTML="<b>Game Over!</b> Check final scores.";
qContainer.style.display="none";
inputGroup.style.display="none";
btnNext.style.display="none";
btnStart.style.display="inline-block";
btnStart.innerText="Play Again";
btnReset.style.display="none";
}
}
</script>
</body>
</html>
)rawliteral";

// ==========================================
// 🛠️ HELPER FUNCTIONS
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
  doc["qText"] = quiz[currentQuestionIdx].text;
  
  JsonArray jsonOpts = doc.createNestedArray("qOptions");
  for (int i = 0; i < 4; i++) {
    jsonOpts.add(quiz[currentQuestionIdx].options[i]);
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
// 📡 WEBSOCKET HANDLER
// ==========================================
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0; // Null-terminate string
    
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, (char*)data);
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
// 🚀 ARDUINO SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // Initialize Pins
  pinMode(P1_PIN, INPUT); // CD4013 provides hard HIGH/LOW, no pullup needed
  pinMode(P2_PIN, INPUT);
  pinMode(P3_PIN, INPUT);
  pinMode(P4_PIN, INPUT);
  
  pinMode(RESET_PIN, OUTPUT);
  pinMode(GAME_RESET_PIN, INPUT_PULLUP);
  resetBuzzer(); // Clear any phantom latches on boot

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

  // Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Attach WebSocket
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Start Server
  server.begin();
  Serial.println("HTTP Server Started");
}

// ==========================================
// 🔄 ARDUINO MAIN LOOP
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
