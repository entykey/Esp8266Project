// Arduino IDE Board: NodeMCU 1.0 (ESP-12E Module)
// Make sure CH340 driver is installed on your machine !
// Port (OSX): /dev/cu.usbserial-0001 Serial Port (USB)
// TIPS: try to unflug and flug it back to detect it.

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <Hash.h>


// Replace with your network credentials
const char* ssid     = "Tuan hay ho ESP8266";
const char* password = "password";

// Set web server port number to 80 (default)
WiFiServer server(80);

// Set web server port number to 81
WebSocketsServer webSocket = WebSocketsServer(81);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output5State = "off";
String output4State = "off";

// Assign output variables to GPIO pins
const int output5 = 5;
const int output4 = 4;

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

// Set Static IP address
IPAddress local_IP(192, 168, 1, 184);
// Set Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   // optional
IPAddress secondaryDNS(8, 8, 4, 4); // optional

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            // send message back to client
            webSocket.sendTXT(num, "Hi client, this is ESP8266 server.");
        }
            break;
        case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", num, payload);

            // Process the received text message here

            break;
    }
}

void setup() {
    Serial.begin(9600);

    // Configures static IP address
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
      Serial.println("STA Failed to configure");
    }

    // set as soft ap
    WiFi.softAPConfig(local_IP, local_IP, IPAddress(255, 255, 255, 0)); //set Static IP gateway on NodeMCU
    WiFi.softAP(ssid, password); //turn on WIFI
    
    // Connect to Wi-Fi network with SSID and password
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    // Wait some time to connect to wifi
    for(int i = 0; i < 15 && WiFi.status() != WL_CONNECTED; i++) {
        Serial.print(".");
        delay(1000);
    }

    // Print local IP address and start web server
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("ESP Board MAC Address:  ");
    Serial.println(WiFi.macAddress());
    server.begin();

    // Start WebSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    if(MDNS.begin("esp8266")) {
        Serial.println("MDNS responder started");
    }

    // Add service to MDNS
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);
}

void loop() {
    // Handle WebSocket events
    webSocket.loop();

    // Handle HTTP requests
    WiFiClient client = server.available();
    if (client) {
        handleClient(client);
    }
}

void handleClient(WiFiClient client) {
    // Extract client IP address
    String clientIP = client.remoteIP().toString();
    
    Serial.print("New Client: ");
    Serial.print(clientIP);

    String currentLine = "";
    currentTime = millis();
    previousTime = currentTime;

    while (client.connected() && currentTime - previousTime <= timeoutTime) {
        currentTime = millis();
        if (client.available()) {
            char c = client.read();
            Serial.write(c);
            header += c;
            if (c == '\n') {
                if (currentLine.length() == 0) {

                    // general html content (Really slow !!!, try AsyncWebServer instead !)
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-type:text/html");
                    client.println("Connection: close");
                    client.println();

                    client.println("<!DOCTYPE html><html>");
                    client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                    client.println("<link rel=\"icon\" href=\"data:,\">");
                    client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
                    client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
                    client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
                    client.println(".button2 {background-color: #77878A;}</style></head>");
                    client.println("<body><h1>ESP8266 Web Server</h1>");
                    client.println("<p>WebSocket server is running at ws://192.168.1.184:81</p>");
                    
                    // JavaScript for WebSocket conn handling
                    client.println("<script>");
                    client.println("var ws = new WebSocket('ws://192.168.1.184:81');");
                    client.println("ws.onopen = function() {");
                    client.println("  console.log('WebSocket connected');");
                    client.println("};");
                    client.println("ws.onmessage = function(event) {");
                    client.println("  console.log('WebSocket message:', event.data);");
                    client.println("};");
                    client.println("function sendMessage() {");
                    client.println("  ws.send('hi');");
                    client.println("}");
                    client.println("</script>");

                    // Button to send message
                    client.println("<button class=\"button\" onclick=\"sendMessage()\">Send Message</button>");

                    client.println("</body></html>");

                    client.println();
                    break;
                } else {
                    currentLine = "";
                }
            } else if (c != '\r') {
                currentLine += c;
            }
        }
    }

    header = "";
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
}