// Arduino IDE Board: NodeMCU 1.0 (ESP-12E Module)
// Make sure CH340 driver is installed on your machine !
// Port (OSX): /dev/cu.usbserial-0001 Serial Port (USB)
// TIPS: try to unflug and flug it back to detect it.

//*
// Nguyen Huu Anh Tuan 16-05-2024 nighty-20240308
// 1. builtin led, websocket + stepper motor assigned (AccelStepper imported) => still works
// 2. modifed websocket even handler => still works
// 3. added html page => worked now !!!
// 4. added lcd and first print after initialization => worked
// 5. buzzer worked
// 6. assign relay to SD2 (GPIO9) => boot looping
// 7. found out, re-assigned relay to SD3 (GPIO10) => worked
// STATUS: all features working fine. Not added crystal i2c yet
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h> // to make crystal_i2c works
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h> // Include the AccelStepper library

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Replace with your network credentials
const char* ssid = "Tuan hay ho";
const char* password = "password";

// Set your Static IP address
IPAddress local_IP(192, 168, 1, 184);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);

// built-in led
int ledPin = 2;
bool ledState = HIGH;

// relay 5v (assigned to SD2 (gpio09) cause boot looping)
#define RELAY_PIN 10 // SD3 (GPIO10)
bool relayState = LOW;


// lcd i2c
// LiquidCrystal_I2C lcd(0x3F,16,2);    // works for Arduino 
// in ESP8266, if lcd is not print then use this 0x27..  
LiquidCrystal_I2C lcd(0x27, 16, 2); // SDA to D4, SCL to D3

// stepper motor
#define MotorInterfaceType 8
#define MP1  D1 // IN1 on the ULN2003
#define MP2  D2 // IN2 on the ULN2003
#define MP3  D5 // IN3 on the ULN2003
#define MP4  D6 // IN4 on the ULN2003
const int SPR = 2048; // Steps per revolution
AccelStepper stepper = AccelStepper(MotorInterfaceType, MP1, MP3, MP2, MP4);

bool motorRotating = false;
int currentPosition = 0; // 0 for closed, 3 for fully open

// HW-038 Water sensor
#define POWER_PIN D7  // The ESP8266 pin that provides the power to the rain sensor
#define AO_PIN    A0  // The ESP8266 pin connected to AO pin of the rain sensor

unsigned long lastWaterCheck = 0;
const unsigned long waterCheckInterval = 1000;
bool isRaining = false;

// buzzer
#define BUZZER_PIN D8

// also rotate stepper once detected rain
void broadcastWaterSensorValue() {
  digitalWrite(POWER_PIN, HIGH);  // turn the rain sensor's power ON
  delay(10);                      // wait 10 milliseconds
  int rainValue = analogRead(AO_PIN);
  digitalWrite(POWER_PIN, LOW);  // turn the rain sensor's power OFF

  Serial.println(rainValue);  // print out the analog value

  // Display the rain value on the LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Rain Value: ");
  lcd.setCursor(0, 1);
  lcd.print(rainValue);

  // Check if the rain value exceeds the threshold of 64
  if (rainValue > 64 && !isRaining) {
    isRaining = true;
    webSocket.broadcastTXT("isRaining=true");
    beep(2);
    closeCurtain();
  } else if (rainValue <= 64 && isRaining) {
    isRaining = false;
    beep(2);
    webSocket.broadcastTXT("isRaining=false");
    openCurtain();
  }
}

void closeCurtain() {
  char responseMsg[15];
  if (!motorRotating) {
                    motorRotating = true;
                    strcpy(responseMsg, "MOTOR_ON");
                    webSocket.broadcastTXT(responseMsg);
                    rotateStepper(-3); // Close the curtain
                    strcpy(responseMsg, "MOTOR_OFF");
                    webSocket.broadcastTXT(responseMsg);
                    strcpy(responseMsg, currentPosition == 3 ? "CURTAIN_OPEN" : "CURTAIN_CLOSED");
                    webSocket.broadcastTXT(responseMsg);
                    flashLED(2);
                }
  else {
    Serial.println("Motor is busy rotating, try again later");
  }
}

void openCurtain()
{
    char responseMsg[15];
    if (!motorRotating)
    {
        motorRotating = true;
        strcpy(responseMsg, "MOTOR_ON");
        webSocket.broadcastTXT(responseMsg);
        rotateStepper(3); // Open the curtain
        strcpy(responseMsg, "MOTOR_OFF");
        webSocket.broadcastTXT(responseMsg);
        strcpy(responseMsg, currentPosition == 3 ? "CURTAIN_OPEN" : "CURTAIN_CLOSED");
        webSocket.broadcastTXT(responseMsg);
        flashLED(2);
    }
    else
    {
        Serial.println("Motor is busy rotating, try again later");
    }
}

void rotateStepper(int rounds) {
    char responseMsg[15];
    if ((rounds > 0 && currentPosition == 3) || (rounds < 0 && currentPosition == 0)) {
        Serial.printf("[rotateStepper] already in position.\n");
        motorRotating = false;
        // strcpy(responseMsg, rounds > 0 ? "ALREADY_OPEN" : "ALREADY_CLOSED");
        webSocket.broadcastTXT(rounds > 0 ? "ALREADY_OPEN" : "ALREADY_CLOSED");
        return;
    }
    
    Serial.printf("[rotateStepper] working...\n");
    stepper.moveTo(stepper.currentPosition() + rounds * SPR);
    stepper.runToPosition();
    currentPosition += rounds > 0 ? 3 : -3;
    motorRotating = false;
    Serial.printf("[rotateStepper] done...\n");
}

void flashLED(int times) {
  for (int i = 0; i < times; ++i) {
    digitalWrite(ledPin, HIGH);
    delay(80);
    digitalWrite(ledPin, LOW);
    delay(80);
    digitalWrite(ledPin, HIGH);
    delay(80);
    digitalWrite(ledPin, LOW);
    delay(80);
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    char responseMsg[15];
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            // flashLED(2);
            beep(2);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
            // flashLED(2);
            beep(2);
            strcpy(responseMsg, ledState ? "ON" : "OFF");
            webSocket.sendTXT(num, responseMsg);
            strcpy(responseMsg, motorRotating ? "MOTOR_ON" : "MOTOR_OFF");
            webSocket.sendTXT(num, responseMsg);
            // webSocket.sendTXT(num, isRaining ? "isRaining:true" : "isRaining:false");
            webSocket.sendTXT(num, "Connected");
        }
        break;
        case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", num, payload);
            if (strcmp((const char*)payload, "toggle") == 0) {
                ledState = !ledState;
                beep(2);
                digitalWrite(ledPin, ledState ? LOW : HIGH);
                strcpy(responseMsg, ledState ? "ON" : "OFF");
                webSocket.broadcastTXT(responseMsg);
            } else if (strcmp((const char*)payload, "open") == 0) {
                beep(2);
                openCurtain();
            } else if (strcmp((const char*)payload, "close") == 0) {
                beep(2);
                closeCurtain();
            } else if (strcmp((const char*)payload, "relaytoggle") == 0) {
                beep(2);
                Serial.println("toggling relay");
                relayState = !relayState; // Update the relay state
                digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);
                strcpy(responseMsg, relayState ? "RELAY_ON" : "RELAY_OFF");
                webSocket.broadcastTXT(responseMsg);
            }
            break;
    }
}

void beep(int times) {
  for (int i = 0; i < times; ++i) {
    //   tone(BUZZER_PIN,13500);
    //   delay(80);
    // tone(BUZZER_PIN,13500);
    digitalWrite(BUZZER_PIN,HIGH);
    delay(80);
    digitalWrite(BUZZER_PIN,LOW);
    delay(80);
  }
}

void handleRoot() {
    server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
        }
        button {
            padding: 15px 25px;
            font-size: 16px;
            color: white;
            background-color: green;
            border: none;
            border-radius: 5px;
            margin: 10px;
            cursor: pointer;
        }
        button:disabled {
            background-color: lightgrey;
            cursor: not-allowed;
        }
        #toggleButton { background-color: red; }
        #openButton, #closeButton { background-color: grey; }
        #status, #ledState, #motorStatus, #curtainStatus, #rainStatusDiv {
            font-size: 18px;
            margin: 10px 0;
        }
        .status-green {
            color: green;
        }
        .status-red {
            color: red;
        }
    </style>
</head>
<body>
    <h1>ESP8266 Web Server</h1>
    <div id='status'>WebSocket Status: Not Connected</div>
    <p>Built-in LED State: <span id="ledState">Unknown</span></p>
    <p>Relay State: <span id="relayState">Unknown</span></p>
    <button id="toggleButton" onclick="toggleLED()">Toggle LED</button>
    <button id="relayToggleButton" onclick="toggleRelay()">Toggle Relay</button>
    <button id="openButton" onclick="openCurtain()">Open Curtain</button>
    <button id="closeButton" onclick="closeCurtain()">Close Curtain</button>
    <div id='motorStatus'>Motor Status: Idle</div>
    <div id='curtainStatus'>Curtain Status: Unknown</div>
    <div id='rainStatusDiv'>Rain Status: Unknown</div>
    <script>
        var ws = new WebSocket('ws://' + window.location.hostname + ':81');
        ws.onopen = function() {
            const statusElement = document.getElementById('status');
            statusElement.innerText = 'WebSocket Status: Connected';
        };
        ws.onmessage = function(event) {
            var stateSpan = document.getElementById('ledState');
            var relayStateSpan = document.getElementById('relayState');
            var motorStatusDiv = document.getElementById('motorStatus');
            var curtainStatusDiv = document.getElementById('curtainStatus');
            var rainStatusDiv = document.getElementById('rainStatusDiv');
            var toggleButton = document.getElementById('toggleButton');
            var toggleRelayButton = document.getElementById('relayToggleButton');
            var openButton = document.getElementById('openButton');
            var closeButton = document.getElementById('closeButton');
            if (event.data === 'ON') {
                stateSpan.textContent = 'ON';
                toggleButton.style.backgroundColor = 'green';
            } else if (event.data === 'OFF') {
                stateSpan.textContent = 'OFF';
                toggleButton.style.backgroundColor = 'red';
            } else if (event.data === 'RELAY_ON') {
                relayStateSpan.textContent = 'ON';
                toggleButton.style.backgroundColor = 'green';
            } else if (event.data === 'RELAY_OFF') {
                relayStateSpan.textContent = 'OFF';
                toggleButton.style.backgroundColor = 'red';
            } else if (event.data === 'MOTOR_ON') {
                motorStatusDiv.textContent = 'Motor Status: Rotating';
                openButton.disabled = true;
                closeButton.disabled = true;
            } else if (event.data === 'MOTOR_OFF') {
                motorStatusDiv.textContent = 'Motor Status: Idle';
                openButton.disabled = false;
                closeButton.disabled = false;
            } else if (event.data === 'isRaining=true') {
                rainStatusDiv.textContent = 'Rain Status: Raining';
                rainStatusDiv.classList.remove('status-green');
                rainStatusDiv.classList.add('status-red');
                alert("It's raining!");
            } else if (event.data === 'isRaining=false') {
                rainStatusDiv.textContent = 'Rain Status: Not Raining';
                rainStatusDiv.classList.remove('status-red');
                rainStatusDiv.classList.add('status-green');
            } else if (event.data === 'CURTAIN_OPEN') {
                curtainStatusDiv.textContent = 'Curtain Status: Open';
                curtainStatusDiv.classList.remove('status-red');
                curtainStatusDiv.classList.add('status-green');
            } else if (event.data === 'CURTAIN_CLOSED') {
                curtainStatusDiv.textContent = 'Curtain Status: Closed';
                curtainStatusDiv.classList.remove('status-green');
                curtainStatusDiv.classList.add('status-red');
            } else if (event.data === 'ALREADY_OPEN') {
                curtainStatusDiv.textContent = 'Curtain Status: Already Opened';
                curtainStatusDiv.classList.remove('status-red');
                curtainStatusDiv.classList.add('status-green');
            } else if (event.data === 'ALREADY_OPEN') {
                curtainStatusDiv.textContent = 'Curtain Status: Already Closed';
                curtainStatusDiv.classList.remove('status-green');
                curtainStatusDiv.classList.add('status-red');
            }
        };
        function toggleLED() {
            ws.send('toggle');
        }
        function toggleRelay() {
            ws.send('relaytoggle');
        }
        function openCurtain() {
            ws.send('open');
        }
        function closeCurtain() {
            ws.send('close');
        }
    </script>
</body>
</html>
    )rawliteral");
}

void setup() {
  Serial.begin(9600);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(POWER_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  beep(1);


  pinMode(ledPin, OUTPUT);
  Wire.begin(2,0);

  // Initialize the LCD
  lcd.init();
  lcd.backlight();
  lcd.print("Setting up"); // (without this, later lines won't print anything !!!)

  stepper.setMaxSpeed(1200);
  stepper.setAcceleration(200);

  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  WiFi.softAPConfig(local_IP, local_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);

  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  for (int i = 0; i < 15 && WiFi.status() != WL_CONNECTED; i++) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());
  

  server.on("/", HTTP_GET, handleRoot);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  MDNS.begin("esp8266");
  MDNS.addService("http", "tcp", 80);
  MDNS.addService("ws", "tcp", 81);

  // the below lines not gonna get executed !!!
  // Print a message to the LCD.
  lcd.setCursor(0, 0);
  lcd.print("ESP8266 Web");
  lcd.setCursor(0, 1);
  lcd.print("Server Ready!");
}

void loop() {
  server.handleClient();
  webSocket.loop();

  if (millis() - lastWaterCheck >= waterCheckInterval) {
    lastWaterCheck = millis();
    broadcastWaterSensorValue();
  }
}