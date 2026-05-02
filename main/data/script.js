var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
window.addEventListener('load', initWebSocket);
function initWebSocket() {
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}
function onOpen(event) { console.log('Connection opened'); }
function onClose(event) { setTimeout(initWebSocket, 2000); }
function onMessage(event) {
    let data = JSON.parse(event.data);
    updateUI(data);
}
function sendAction(actionStr) {
    websocket.send(JSON.stringify({ action: actionStr }));
}
function confirmReset() {
    if (confirm("Are you sure you want to reset the game? All scores will be lost.")) {
        sendAction('reset');
    }
}
function submitAnswer(optIdx) {
    websocket.send(JSON.stringify({ action: 'submit', answer: optIdx }));
}
function updateUI(data) {
    for (let i = 1; i <= 4; i++) {
        document.getElementById('s' + i).innerText = data.scores[i - 1];
        document.getElementById('p' + i + '-card').classList.remove('active');
    }
    let state = data.state;
    let statusDiv = document.getElementById('status-bar');
    let qContainer = document.getElementById('q-container');
    let inputGroup = document.getElementById('input-group');
    let btnStart = document.getElementById('btn-start');
    let btnNext = document.getElementById('btn-next');
    let btnReset = document.getElementById('btn-reset');
    let btnAi = document.getElementById('btn-ai');
    let resultMsg = document.getElementById('result-msg');
    if (state > 0 && state < 4) {
        qContainer.style.display = "block";
        document.getElementById('q-num').innerText = (data.qIdx + 1);
        document.getElementById('q-text').innerText = data.qText;
        if (data.qOptions) {
            document.getElementById('opt0').innerText = data.qOptions[0];
            document.getElementById('opt1').innerText = data.qOptions[1];
            document.getElementById('opt2').innerText = data.qOptions[2];
            document.getElementById('opt3').innerText = data.qOptions[3];
        }
        btnStart.style.display = "none";
        if (btnAi) btnAi.style.display = "none";
    }
    if (state === 0) {
        statusDiv.className = "status-msg waiting";
        statusDiv.innerText = "Press Start Game to begin!";
        qContainer.style.display = "none";
        inputGroup.style.display = "none";
        btnNext.style.display = "none";
        btnStart.style.display = "inline-block";
        btnStart.innerText = "Start Game";
        btnReset.style.display = "none";
        if (btnAi) btnAi.style.display = "inline-block";
    } else if (state === 1) {
        statusDiv.className = "status-msg waiting";
        statusDiv.innerText = "Waiting for players to buzz...";
        inputGroup.style.display = "block";
        document.getElementById('mcq-options').style.display = "grid";
        btnNext.style.display = "none";
        resultMsg.innerText = "";
        btnReset.style.display = "inline-block";
    } else if (state === 2) {
        statusDiv.className = "status-msg buzzed";
        statusDiv.innerText = "Player " + data.activePlayer + " buzzed! Awaiting answer...";
        document.getElementById('p' + data.activePlayer + '-card').classList.add('active');
        inputGroup.style.display = "block";
        document.getElementById('mcq-options').style.display = "grid";
        btnNext.style.display = "none";
        btnReset.style.display = "inline-block";
    } else if (state === 3) {
        statusDiv.className = "status-msg over";
        statusDiv.innerText = "Round Over";
        btnNext.style.display = "inline-block";
        if (data.isLast) {
            btnNext.innerText = "Finish";
        } else {
            btnNext.innerText = "Next Question";
        }
        resultMsg.innerHTML = data.resultMsg;
        inputGroup.style.display = "block";
        document.getElementById('mcq-options').style.display = "none";
        btnReset.style.display = "inline-block";
    } else if (state === 4) {
        statusDiv.className = "status-msg over";
        statusDiv.innerHTML = "<b>Game Over!</b> Check final scores.";
        qContainer.style.display = "none";
        inputGroup.style.display = "none";
        btnNext.style.display = "none";
        btnStart.style.display = "inline-block";
        btnStart.innerText = "Play Again";
        btnReset.style.display = "none";
        if (btnAi) btnAi.style.display = "inline-block";
    }
}

// AI Generator Logic
function openAIModal() {
    document.getElementById('ai-modal').style.display = 'flex';
}

function closeAIModal() {
    document.getElementById('ai-modal').style.display = 'none';
    document.getElementById('ai-status').innerText = '';
}

async function generateWithAI() {
    let topic = document.getElementById('ai-topic').value.trim();
    let statusEl = document.getElementById('ai-status');

    if (!topic) {
        statusEl.innerText = 'Please enter a topic.';
        return;
    }

    statusEl.innerText = 'Generating 10 questions... Please wait.';
    document.getElementById('btn-gen-ai').disabled = true;

    try {
        let keyRes = await fetch('/api_key');
        if (!keyRes.ok) throw new Error("Failed to load API key from ESP32");
        let key = (await keyRes.text()).trim();

        if (!key || key === "YOUR_OPENROUTER_API_KEY") {
            throw new Error("API Key not configured in secrets.h");
        }

        let prompt = `Generate exactly 10 multiple choice questions about "${topic}".
Return ONLY a valid JSON array of objects. Do not include markdown formatting or backticks.
Each object must have:
- "text": string (the question)
- "options": array of 4 strings (the possible answers)
- "correctOption": integer (0, 1, 2, or 3 representing the index of the correct option)
Example: [{"text": "What is 2+2?", "options": ["1", "2", "3", "4"], "correctOption": 3}]`;

        let response = await fetch("https://openrouter.ai/api/v1/chat/completions", {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${key}`,
                'HTTP-Referer': window.location.href,
                'X-Title': 'ESP Quiz'
            },
            body: JSON.stringify({
                model: "openrouter/free",
                messages: [{ role: "user", content: prompt }],
                temperature: 0.7
            })
        });

        if (!response.ok) {
            let errText = await response.text();
            throw new Error(`API Error: ${response.status} - ${errText}`);
        }

        let data = await response.json();
        let rawJson = data.choices[0].message.content;

        // Remove markdown code blocks if the AI returns them
        rawJson = rawJson.replace(/```json/gi, "").replace(/```/g, "").trim();

        let startIndex = rawJson.indexOf('[');
        if (startIndex === -1) throw new Error("No JSON array found in response.");
        rawJson = rawJson.substring(startIndex);

        let questions = null;
        let str = rawJson;
        while (str.length > 0) {
            let endIndex = str.lastIndexOf(']');
            if (endIndex === -1) break;

            try {
                questions = JSON.parse(str.substring(0, endIndex + 1));
                break; // Successfully parsed!
            } catch (e) {
                // If it fails, it might be trailing text containing a ']', so strip it and try again
                str = str.substring(0, endIndex);
            }
        }

        if (!questions || !Array.isArray(questions) || questions.length !== 10) {
            throw new Error("Invalid format or question count received from AI.");
        }

        // Send questions one by one to prevent ESP32 memory/fragmentation issues
        for (let i = 0; i < questions.length; i++) {
            websocket.send(JSON.stringify({
                action: 'update_question',
                index: i,
                question: questions[i]
            }));
            // Short delay to avoid flooding the ESP32 async buffer
            await new Promise(r => setTimeout(r, 50));
        }

        // Signal completion
        websocket.send(JSON.stringify({ action: 'questions_updated' }));
        closeAIModal();
        alert("Questions updated successfully! Press Start Game to begin.");

    } catch (error) {
        console.error(error);
        statusEl.innerText = `Error: ${error.message}`;
    } finally {
        document.getElementById('btn-gen-ai').disabled = false;
    }
}
