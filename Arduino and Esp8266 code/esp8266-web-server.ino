/*
 * ESP8266 Web Server for Arduino Gaming System
 * Hardware: ESP8266 (NodeMCU/Wemos D1 Mini) with micro USB power
 * Communication: Serial to Arduino for game data
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

// ===== WIFI CONFIGURATION =====
const char* ssid = "YourWiFiName";        // Change this to your WiFi name
const char* password = "YourWiFiPassword"; // Change this to your WiFi password
const char* hostname = "arduino-gaming";

// ===== WEB SERVER =====
ESP8266WebServer server(80);

// ===== SERIAL COMMUNICATION =====
String serialBuffer = "";
String lastGameData = "{}";
String lastHighScores = "[]";
String lastStats = "{}";
bool arduinoConnected = false;
unsigned long lastArduinoResponse = 0;
unsigned long lastDataRequest = 0;
const unsigned long dataRequestInterval = 2000; // Request data every 2 seconds

// ===== SETUP =====
void setup() {
  Serial.begin(9600);
  
  // Initialize file system
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
  }
  
  // Initialize WiFi
  initWiFi();
  
  // Setup web server
  setupWebServer();
  
  Serial.println("ESP8266_READY");
}

// ===== MAIN LOOP =====
void loop() {
  server.handleClient();
  MDNS.update();
  
  // Handle serial communication with Arduino
  handleSerialCommunication();
  
  // Request data from Arduino periodically
  if (millis() - lastDataRequest > dataRequestInterval) {
    requestDataFromArduino();
    lastDataRequest = millis();
  }
  
  // Check Arduino connection status
  if (millis() - lastArduinoResponse > 5000) {
    arduinoConnected = false;
  }
  
  yield(); // Allow ESP8266 to handle WiFi tasks
}

// ===== WIFI FUNCTIONS =====
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostname);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Hostname: ");
    Serial.println(hostname);
    
    // Setup mDNS
    if (MDNS.begin(hostname)) {
      MDNS.addService("http", "tcp", 80);
      Serial.println("mDNS responder started");
    }
  } else {
    Serial.println("");
    Serial.println("WiFi connection failed");
  }
}

void setupWebServer() {
  // Serve main dashboard page
  server.on("/", HTTP_GET, []() {
    if (LittleFS.exists("/index.html")) {
      File file = LittleFS.open("/index.html", "r");
      server.streamFile(file, "text/html");
      file.close();
    } else {
      server.send(404, "text/plain", "Web files not found. Please upload web files to LittleFS.");
    }
  });
  
  // Serve CSS
  server.on("/styles.css", HTTP_GET, []() {
    if (LittleFS.exists("/styles.css")) {
      File file = LittleFS.open("/styles.css", "r");
      server.streamFile(file, "text/css");
      file.close();
    } else {
      server.send(404, "text/plain", "CSS file not found");
    }
  });
  
  // Serve JavaScript
  server.on("/app.js", HTTP_GET, []() {
    if (LittleFS.exists("/app.js")) {
      File file = LittleFS.open("/app.js", "r");
      server.streamFile(file, "application/javascript");
      file.close();
    } else {
      server.send(404, "text/plain", "JavaScript file not found");
    }
  });
  
  // API endpoint for system status
  server.on("/api/status", HTTP_GET, []() {
    DynamicJsonDocument doc(256);
    doc["status"] = arduinoConnected ? "online" : "offline";
    doc["uptime"] = millis();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["wifiSignal"] = WiFi.RSSI();
    doc["arduinoConnected"] = arduinoConnected;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });
  
  // API endpoint for game data
  server.on("/api/gamedata", HTTP_GET, []() {
    // Add system info to game data
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, lastGameData);
    
    doc["wifiSignal"] = WiFi.RSSI();
    doc["espUptime"] = millis();
    doc["freeMemory"] = ESP.getFreeHeap();
    doc["arduinoConnected"] = arduinoConnected;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });
  
  // API endpoint for high scores
  server.on("/api/highscores", HTTP_GET, []() {
    DynamicJsonDocument doc(2048);
    doc["scores"] = serialized(lastHighScores);
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });
  
  // API endpoint for game statistics
  server.on("/api/stats", HTTP_GET, []() {
    server.send(200, "application/json", lastStats);
  });
  
  // API endpoint for web controls
  server.on("/api/control", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, server.arg("plain"));
      
      String action = doc["action"];
      
      // Send command to Arduino
      Serial.println("CMD:" + action);
      
      server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      server.send(400, "application/json", "{\"error\":\"No data\"}");
    }
  });
  
  // API endpoint for LED control
  server.on("/api/led", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(256);
      deserializeJson(doc, server.arg("plain"));
      
      // Extract LED control parameters
      int r = doc["r"] | 0;
      int g = doc["g"] | 0;
      int b = doc["b"] | 0;
      int brightness = doc["brightness"] | 100;
      String mode = doc["mode"] | "solid";
      bool isOn = doc["isOn"] | false;
      
      // Create LED control command
      String ledCommand = "LED_CONTROL:" + String(r) + "," + 
                         String(g) + "," + 
                         String(b) + "," + 
                         String(brightness) + "," + 
                         mode + "," + 
                         String(isOn ? "true" : "false");
      
      // Send command to Arduino
      Serial.println(ledCommand);
      
      server.send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
      server.send(400, "application/json", "{\"error\":\"No data\"}");
    }
  });
  
  // File upload endpoint for web files
  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "Upload complete");
  }, handleFileUpload);
  
  // Handle 404
  server.onNotFound([]() {
    server.send(404, "text/plain", "File not found");
  });
  
  // Start server
  server.begin();
  Serial.println("Web server started");
}

// ===== FILE UPLOAD HANDLER =====
void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    
    Serial.print("Upload Start: "); Serial.println(filename);
    
    // Open file for writing
    File file = LittleFS.open(filename, "w");
    if (!file) {
      Serial.println("Failed to open file for writing");
      return;
    }
    file.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    // Write chunk to file
    File file = LittleFS.open("/" + upload.filename, "a");
    if (file) {
      file.write(upload.buf, upload.currentSize);
      file.close();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    Serial.print("Upload End: "); Serial.println(upload.totalSize);
  }
}

// ===== SERIAL COMMUNICATION =====
void handleSerialCommunication() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      processSerialMessage(serialBuffer);
      serialBuffer = "";
    } else {
      serialBuffer += c;
    }
  }
}

void processSerialMessage(String message) {
  message.trim();
  
  if (message.startsWith("DATA:")) {
    // Game data from Arduino
    lastGameData = message.substring(5);
    lastArduinoResponse = millis();
    arduinoConnected = true;
  }
  else if (message.startsWith("SCORES:")) {
    // High scores from Arduino
    lastHighScores = message.substring(7);
    lastArduinoResponse = millis();
    arduinoConnected = true;
  }
  else if (message.startsWith("STATS:")) {
    // Statistics from Arduino
    lastStats = message.substring(6);
    lastArduinoResponse = millis();
    arduinoConnected = true;
  }
  else if (message == "ARDUINO_READY") {
    // Arduino is ready
    arduinoConnected = true;
    lastArduinoResponse = millis();
    Serial.println("ESP8266_READY");
  }
}

void requestDataFromArduino() {
  if (arduinoConnected) {
    Serial.println("GET_DATA");
    delay(50);
    Serial.println("GET_SCORES");
    delay(50);
    Serial.println("GET_STATS");
  }
}
