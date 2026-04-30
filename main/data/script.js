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
    } else if (state === 1) {
        statusDiv.className = "status-msg waiting";
        statusDiv.innerText = "Waiting for players to buzz...";
        inputGroup.style.display = "none";
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
    }
}
