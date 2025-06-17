#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <FS.h>

// WiFi Configuration - CHANGE THESE TO YOUR NETWORK
const char* ssid = "Norbort";
const char* password = "@N0rb0rt";

// Web Server and WebSocket
ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

// Game Data Structure
struct GameData {
  String currentGame = "menu";
  String gameState = "idle";
  int flappyScore = 0;
  int flappyHigh = 0;
  int snakeScore = 0;
  int snakeHigh = 0;
  int snakeLength = 2;
  int pongScore = 0;
  int pongHigh = 0;
  unsigned long uptime = 0;
  int freeMemory = 0;
  int wifiSignal = 0;
  bool arduinoConnected = false;
  
  // RGB LED data
  int ledR = 0;
  int ledG = 100;
  int ledB = 255;
  int ledBrightness = 200;
  String ledMode = "pulse";
  bool ledOn = true;
  bool ledGameSync = false;
} gameData;

// High Scores
struct HighScore {
  String player;
  String game;
  int score;
};

HighScore highScores[20];
int highScoreCount = 0;

// Timing variables
unsigned long lastArduinoData = 0;
unsigned long lastBroadcast = 0;

void setup() {
  Serial.begin(9600);
  
  // Initialize SPIFFS for file system
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialization failed!");
    Serial.println("Make sure you've uploaded the data files using 'ESP8266 Sketch Data Upload'");
    return;
  }
  Serial.println("SPIFFS initialized successfully");
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFailed to connect to WiFi!");
    Serial.println("Please check your credentials and try again.");
    return;
  }
  
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Access dashboard at: http://");
  Serial.println(WiFi.localIP());
  
  // Setup web server routes
  setupWebServer();
  
  // Start WebSocket server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  // Start web server
  server.begin();
  Serial.println("Web server started");
  Serial.println("WebSocket server started on port 81");
  
  loadHighScores();
}

void loop() {
  server.handleClient();
  webSocket.loop();
  
  // Handle Arduino communication
  handleArduinoData();
  
  // Update system data
  updateSystemData();
  
  // WiFi reconnection check
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 30000) { // Check every 30 seconds
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      WiFi.begin(ssid, password);
    }
    lastWiFiCheck = millis();
  }
  
  // Send periodic updates to connected clients
  if (millis() - lastBroadcast > 1000) {
    broadcastGameData();
    lastBroadcast = millis();
  }
  
  delay(10);
}

void setupWebServer() {
  // Serve the main HTML file from SPIFFS
  server.on("/", HTTP_GET, []() {
    File file = SPIFFS.open("/index.html", "r");
    if (file) {
      server.streamFile(file, "text/html");
      file.close();
    } else {
      server.send(404, "text/plain", "index.html not found. Please upload web files to SPIFFS using 'Tools > ESP8266 Sketch Data Upload'");
    }
  });
  
  // Serve CSS file from SPIFFS
  server.on("/styles.css", HTTP_GET, []() {
    File file = SPIFFS.open("/styles.css", "r");
    if (file) {
      server.streamFile(file, "text/css");
      file.close();
    } else {
      server.send(404, "text/plain", "styles.css not found");
    }
  });
  
  // Serve JavaScript file from SPIFFS
  server.on("/app.js", HTTP_GET, []() {
    File file = SPIFFS.open("/app.js", "r");
    if (file) {
      server.streamFile(file, "application/javascript");
      file.close();
    } else {
      server.send(404, "text/plain", "app.js not found");
    }
  });
  
  // API endpoints
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/gamedata", HTTP_GET, handleGameData);
  server.on("/api/highscores", HTTP_GET, handleHighScores);
  server.on("/api/control", HTTP_POST, handleControl);
  
  // Enable CORS for all routes
  server.enableCORS(true);
  
  // Handle file not found
  server.onNotFound([]() {
    String path = server.uri();
    Serial.println("File not found: " + path);
    server.send(404, "text/plain", "File not found: " + path);
  });
}

void handleStatus() {
  DynamicJsonDocument doc(300);
  doc["arduinoConnected"] = gameData.arduinoConnected;
  doc["wifiSignal"] = WiFi.RSSI();
  doc["uptime"] = millis();
  doc["freeMemory"] = ESP.getFreeHeap();
  doc["ipAddress"] = WiFi.localIP().toString();
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleGameData() {
  DynamicJsonDocument doc(600);
  doc["currentGame"] = gameData.currentGame;
  doc["gameState"] = gameData.gameState;
  doc["flappyScore"] = gameData.flappyScore;
  doc["flappyHigh"] = gameData.flappyHigh;
  doc["snakeScore"] = gameData.snakeScore;
  doc["snakeHigh"] = gameData.snakeHigh;
  doc["snakeLength"] = gameData.snakeLength;
  doc["pongScore"] = gameData.pongScore;
  doc["pongHigh"] = gameData.pongHigh;
  doc["uptime"] = millis();
  doc["freeMemory"] = ESP.getFreeHeap();
  doc["wifiSignal"] = WiFi.RSSI();
  doc["arduinoConnected"] = gameData.arduinoConnected;
  
  // LED data
  doc["ledR"] = gameData.ledR;
  doc["ledG"] = gameData.ledG;
  doc["ledB"] = gameData.ledB;
  doc["ledBrightness"] = gameData.ledBrightness;
  doc["ledMode"] = gameData.ledMode;
  doc["ledOn"] = gameData.ledOn;
  doc["ledGameSync"] = gameData.ledGameSync;
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleHighScores() {
  DynamicJsonDocument doc(2048);
  JsonArray scores = doc.createNestedArray("scores");
  
  for (int i = 0; i < highScoreCount; i++) {
    JsonObject score = scores.createNestedObject();
    score["player"] = highScores[i].player;
    score["game"] = highScores[i].game;
    score["score"] = highScores[i].score;
  }
  
  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleControl() {
  if (server.hasArg("plain")) {
    DynamicJsonDocument doc(200);
    deserializeJson(doc, server.arg("plain"));
    
    String action = doc["action"];
    
    // Send control command to Arduino
    Serial.print("CONTROL:");
    Serial.println(action);
    
    Serial.println("Sent control: " + action);
    
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400, "application/json", "{\"error\":\"No data\"}");
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("WebSocket [%u] Disconnected!\n", num);
      break;
      
    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("WebSocket [%u] Connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      
      // Send initial data
      broadcastGameData();
      break;
    }
    
    case WStype_TEXT: {
      String message = String((char*)payload);
      Serial.println("WebSocket message: " + message);
      
      DynamicJsonDocument doc(512);
      deserializeJson(doc, message);
      
      if (doc["type"] == "LED_CONTROL") {
        handleLEDControl(doc);
      } else if (doc["type"] == "CONTROL") {
        String action = doc["action"];
        Serial.print("CONTROL:");
        Serial.println(action);
      }
      break;
    }
    
    default:
      break;
  }
}

void handleLEDControl(DynamicJsonDocument& doc) {
  // Update local LED data
  gameData.ledR = doc["r"];
  gameData.ledG = doc["g"];
  gameData.ledB = doc["b"];
  gameData.ledBrightness = doc["brightness"];
  gameData.ledMode = doc["mode"].as<String>();
  gameData.ledOn = doc["isOn"];
  gameData.ledGameSync = doc["gameSync"];
  
  // Send LED command to Arduino
  String ledCommand = "LED:";
  ledCommand += String(gameData.ledR) + ",";
  ledCommand += String(gameData.ledG) + ",";
  ledCommand += String(gameData.ledB) + ",";
  ledCommand += String(gameData.ledBrightness) + ",";
  ledCommand += gameData.ledMode + ",";
  ledCommand += String(gameData.ledOn ? 1 : 0) + ",";
  ledCommand += String(gameData.ledGameSync ? 1 : 0);
  
  Serial.println(ledCommand);
  Serial.println("LED control sent to Arduino");
}

void handleArduinoData() {
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();
    
    if (data.startsWith("DATA:")) {
      parseGameData(data.substring(5));
      gameData.arduinoConnected = true;
      lastArduinoData = millis();
    } else {
      Serial.println("Arduino: " + data);
    }
  }
  
  // Check Arduino connection timeout (5 seconds)
  if (millis() - lastArduinoData > 5000) {
    gameData.arduinoConnected = false;
  }
}

void parseGameData(String data) {
  // Parse: "gameType,stateType,score,flappyHigh,snakeHigh,pongHigh,snakeLength,uptime,memory,ledR,ledG,ledB,ledBrightness,ledMode,ledOn,ledGameSync"
  int valueIndex = 0;
  int lastIndex = 0;
  String values[20];
  
  // Split the data by commas
  for (int i = 0; i <= data.length(); i++) {
    if (i == data.length() || data.charAt(i) == ',') {
      if (valueIndex < 20) {
        values[valueIndex] = data.substring(lastIndex, i);
      }
      lastIndex = i + 1;
      valueIndex++;
    }
  }
  
  if (valueIndex >= 9) {
    gameData.currentGame = values[0];
    gameData.gameState = values[1];
    
    // Update current game score
    int currentScore = values[2].toInt();
    if (values[0] == "flappy") {
      gameData.flappyScore = currentScore;
    } else if (values[0] == "snake") {
      gameData.snakeScore = currentScore;
    } else if (values[0] == "pong") {
      gameData.pongScore = currentScore;
    }
    
    // Update high scores
    gameData.flappyHigh = values[3].toInt();
    gameData.snakeHigh = values[4].toInt();
    gameData.pongHigh = values[5].toInt();
    
    // Check for new high scores and save them
    if (currentScore > 0) {
      bool isNewHigh = false;
      String gameName = values[0];
      
      if (gameName == "flappy" && currentScore > gameData.flappyHigh) {
        gameData.flappyHigh = currentScore;
        isNewHigh = true;
      } else if (gameName == "snake" && currentScore > gameData.snakeHigh) {
        gameData.snakeHigh = currentScore;
        isNewHigh = true;
      } else if (gameName == "pong" && currentScore > gameData.pongHigh) {
        gameData.pongHigh = currentScore;
        isNewHigh = true;
      }
      
      if (isNewHigh) {
        addHighScore("Player", gameName, currentScore);
        saveHighScores();
        Serial.println("New high score: " + gameName + " - " + String(currentScore));
      }
    }
    
    if (valueIndex >= 7) gameData.snakeLength = values[6].toInt();
    if (valueIndex >= 8) gameData.uptime = values[7].toInt();
    if (valueIndex >= 9) gameData.freeMemory = values[8].toInt();
    
    // Parse LED data if available
    if (valueIndex >= 16) {
      gameData.ledR = values[9].toInt();
      gameData.ledG = values[10].toInt();
      gameData.ledB = values[11].toInt();
      gameData.ledBrightness = values[12].toInt();
      gameData.ledMode = values[13];
      gameData.ledOn = (values[14] == "1");
      gameData.ledGameSync = (values[15] == "1");
    }
  }
}

void updateSystemData() {
  gameData.wifiSignal = WiFi.RSSI();
}

void broadcastGameData() {
  // Check memory before broadcasting
  if (ESP.getFreeHeap() < 8000) {
    Serial.println("Warning: Low memory - " + String(ESP.getFreeHeap()) + " bytes free");
    return;
  }
  
  DynamicJsonDocument doc(1024);
  doc["type"] = "gameData";
  
  JsonObject payload = doc.createNestedObject("payload");
  payload["currentGame"] = gameData.currentGame;
  payload["gameState"] = gameData.gameState;
  payload["flappyScore"] = gameData.flappyScore;
  payload["flappyHigh"] = gameData.flappyHigh;
  payload["snakeScore"] = gameData.snakeScore;
  payload["snakeHigh"] = gameData.snakeHigh;
  payload["snakeLength"] = gameData.snakeLength;
  payload["pongScore"] = gameData.pongScore;
  payload["pongHigh"] = gameData.pongHigh;
  payload["uptime"] = millis();
  payload["freeMemory"] = ESP.getFreeHeap();
  payload["wifiSignal"] = gameData.wifiSignal;
  payload["arduinoConnected"] = gameData.arduinoConnected;
  
  // LED status
  payload["ledR"] = gameData.ledR;
  payload["ledG"] = gameData.ledG;
  payload["ledB"] = gameData.ledB;
  payload["ledBrightness"] = gameData.ledBrightness;
  payload["ledMode"] = gameData.ledMode;
  payload["ledOn"] = gameData.ledOn;
  payload["ledGameSync"] = gameData.ledGameSync;
  
  String message;
  serializeJson(doc, message);
  webSocket.broadcastTXT(message);
}

void loadHighScores() {
  // Try to load high scores from SPIFFS
  File file = SPIFFS.open("/highscores.json", "r");
  if (file) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, file);
    
    JsonArray scores = doc["scores"];
    highScoreCount = 0;
    
    for (JsonObject score : scores) {
      if (highScoreCount < 20) {
        highScores[highScoreCount].player = score["player"].as<String>();
        highScores[highScoreCount].game = score["game"].as<String>();
        highScores[highScoreCount].score = score["score"];
        highScoreCount++;
      }
    }
    
    file.close();
    Serial.println("High scores loaded from SPIFFS (" + String(highScoreCount) + " scores)");
  } else {
    // Create some default high scores
    addHighScore("Player1", "Flappy Bird", 15);
    addHighScore("Player2", "Snake", 8);
    addHighScore("Player3", "Pong", 12);
    Serial.println("Created default high scores");
  }
}

void addHighScore(String player, String game, int score) {
  // Find the right spot to insert the new high score (sorted by score)
  int insertIndex = highScoreCount;
  
  for (int i = 0; i < highScoreCount; i++) {
    if (score > highScores[i].score) {
      insertIndex = i;
      break;
    }
  }
  
  // Shift existing scores down to make room
  for (int i = min(highScoreCount, 19); i > insertIndex; i--) {
    highScores[i] = highScores[i - 1];
  }
  
  // Insert the new score
  highScores[insertIndex].player = player;
  highScores[insertIndex].game = game;
  highScores[insertIndex].score = score;
  
  if (highScoreCount < 20) {
    highScoreCount++;
  }
}

void saveHighScores() {
  DynamicJsonDocument doc(2048);
  JsonArray scores = doc.createNestedArray("scores");
  
  for (int i = 0; i < highScoreCount; i++) {
    JsonObject score = scores.createNestedObject();
    score["player"] = highScores[i].player;
    score["game"] = highScores[i].game;
    score["score"] = highScores[i].score;
  }
  
  File file = SPIFFS.open("/highscores.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
    Serial.println("High scores saved to SPIFFS");
  } else {
    Serial.println("Failed to save high scores");
  }
}
