#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// ==== CONFIGURATION ====
// Replace with your WiFi credentials:
const char* ssid = "LAPTOP-F3KGRQO2 7931";
const char* password = "9y951=8A";

// Pins - change if needed:
const int irPin = 34;      // IR sensor input pin
const int buzzerPin = 2;  // Buzzer output pin

// Webserver & WebSocket setup
WebServer server(80);
WebSocketsServer webSocket(81);

// Variables for pen detection & instruction steps
bool lastPenOnLine = false;
unsigned long lastSent = 0;
const unsigned long sendInterval = 100;  // ms

int currentStep = 0;
const int requiredOnLineCount = 5;
int onLineCounter = 0;

// Letter drawing instructions
const char* instructions_N[] = {
  "Start at top-left and move diagonally down-right.",
  "Now move straight up.",
  "Finish by moving diagonally down-right again."
};
const int steps_N = sizeof(instructions_N) / sizeof(instructions_N[0]);

const char* instructions_O[] = {
  "Start at top-center, move clockwise around the circle.",
  "Complete the loop to form letter O."
};
const int steps_O = sizeof(instructions_O) / sizeof(instructions_O[0]);

// Select letter to practice - can extend for UI selection later
const char* currentLetter = "N";

// === HTML PAGE ===
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Smart Pen Tutor</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      padding: 30px;
      background: #f3f3f3;
    }
    #letter { font-size: 100px; }
    #instruction { font-size: 22px; margin-top: 20px; }
    #status { font-size: 18px; margin-top: 20px; min-height: 30px; }
    select {
      font-size: 18px;
      padding: 5px 10px;
      margin-top: 20px;
    }
  </style>
</head>
<body>
  <h1>Write Letter: <span id="letter">N</span></h1>
  <label for="letterSelect">Choose Letter: </label>
  <select id="letterSelect">
    <option value="N">N</option>
    <option value="O">O</option>
  </select>
  <div id="instruction">Loading instructions...</div>
  <div id="status">Waiting for pen input...</div>
  <script>
    const letterInstructions = {
      "N": [
        "Start at top-left, move diagonally down-right.",
        "Now move straight up.",
        "Finish by moving diagonally down-right again."
      ],
      "O": [
        "Start at top-center, move clockwise around the circle.",
        "Complete the loop to form letter O."
      ]
    };

    const letterEl = document.getElementById('letter');
    const instructionEl = document.getElementById('instruction');
    const statusEl = document.getElementById('status');
    const letterSelect = document.getElementById('letterSelect');

    let steps = [];
    let currentStep = 0;
    let stepStarted = false;
    let onLineTime = 0;
    const requiredOnLineCount = 5;

    function speak(text) {
      if ('speechSynthesis' in window) {
        const utter = new SpeechSynthesisUtterance(text);
        window.speechSynthesis.cancel();
        window.speechSynthesis.speak(utter);
      } else {
        console.log("Speech synthesis not supported.");
      }
    }

    function updateStep() {
      if (currentStep < steps.length) {
        instructionEl.textContent = Step ${currentStep + 1}: ${steps[currentStep]};
        speak(Step ${currentStep + 1}: ${steps[currentStep]});
        statusEl.textContent = "Waiting for pen input...";
        statusEl.style.color = "green";
      } else {
        instructionEl.textContent = "🎉 Letter Completed! Well done.";
        speak("Congratulations! You have completed the letter.");
        statusEl.textContent = "";
      }
    }

    function loadLetter(letter) {
      letterEl.textContent = letter;
      steps = letterInstructions[letter] || ["No instructions found."];
      currentStep = 0;
      stepStarted = false;
      onLineTime = 0;
      updateStep();
    }

    letterSelect.addEventListener("change", (e) => {
      loadLetter(e.target.value);
      const ws = new WebSocket(ws://${location.hostname}:81/);
      ws.send(letter:${e.target.value});
    });

    loadLetter(letterSelect.value);

    const ws = new WebSocket(ws://${location.hostname}:81/);

    ws.onopen = () => {
      statusEl.style.color = 'green';
      statusEl.textContent = "Connected to pen sensor.";
    };

    ws.onmessage = (event) => {
      const state = event.data;
      if (currentStep >= steps.length) return;

      if (state === 'off') {
        if (!stepStarted) {
          stepStarted = true;
          onLineTime = 1;
        } else {
          onLineTime++;
        }
        statusEl.style.color = 'green';
        statusEl.textContent = "Pen is on the line.";
      } else if (state === 'on') {
        if (stepStarted && onLineTime >= requiredOnLineCount) {
          currentStep++;
          updateStep();
        }
        stepStarted = false;
        onLineTime = 0;
        if (currentStep < steps.length) {
          statusEl.style.color = 'red';
          statusEl.textContent = "⚠️ Pen off the line! Please move back.";
          speak("Pen off the line! Please move back.");
        }
      }
    };

    ws.onerror = () => {
      statusEl.style.color = 'red';
      statusEl.textContent = "WebSocket connection error.";
    };

    ws.onclose = () => {
      statusEl.style.color = 'orange';
      statusEl.textContent = "WebSocket connection closed.";
    };
  </script>
</body>
</html>
)rawliteral";

// === Webserver Handlers ===
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

// WebSocket event handler
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char*)payload).substring(0, length);
    if (msg.startsWith("letter:")) {
      String newLetter = msg.substring(7);
      if (newLetter == "N" || newLetter == "O") {
        currentStep = 0;
        onLineCounter = 0;
        currentLetter = newLetter == "N" ? "N" : "O";
        sendInstruction(currentStep);
      }
    }
  }
}

// Send instruction text via WebSocket
void sendInstruction(int step) {
  if (strcmp(currentLetter, "N") == 0) {
    if (step < steps_N) {
      webSocket.broadcastTXT(String("instruction:") + instructions_N[step]);
    } else {
      webSocket.broadcastTXT("instruction:🎉 Letter Completed! Well done.");
    }
  } else if (strcmp(currentLetter, "O") == 0) {
    if (step < steps_O) {
      webSocket.broadcastTXT(String("instruction:") + instructions_O[step]);
    } else {
      webSocket.broadcastTXT("instruction:🎉 Letter Completed! Well done.");
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(irPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  currentStep = 0;
  sendInstruction(currentStep);
}

void loop() {
  server.handleClient();
  webSocket.loop();

  unsigned long now = millis();
  if (now - lastSent > sendInterval) {
    lastSent = now;
    bool penOnLine = digitalRead(irPin) == LOW;  // LOW means on the line (adjust if sensor logic is inverted)

    if (penOnLine != lastPenOnLine) {
      lastPenOnLine = penOnLine;
      if (penOnLine) {
        webSocket.broadcastTXT("on");
        digitalWrite(buzzerPin, HIGH);  // Buzzer off when on the line
      } else {
        webSocket.broadcastTXT("off");
        digitalWrite(buzzerPin, LOW); // Buzzer on when off the line
      }
    }
  }
}