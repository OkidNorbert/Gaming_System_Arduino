#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <BearSSLHelpers.h>
#include <Hash.h>
#include <vector>
#include <map>

// WiFi Configuration - CHANGE THESE TO YOUR NETWORK
const char* ssid = "okidi6";
const char* password = "warrior30";

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
  int snakeHigh = 0;  // Added missing member
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

// User struct
struct User {
  String username;
  String passwordHash;
};

std::vector<User> users;
std::map<String, String> sessions; // token -> username

// Timing variables
unsigned long lastArduinoData = 0;
unsigned long lastBroadcast = 0;

void setup() {
  Serial.begin(9600);
  
  // Initialize SPIFFS for file system
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialization failed!");
    return;
  }
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
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
  loadUsers();
}

void loop() {
  server.handleClient();
  webSocket.loop();
  
  // Handle Arduino communication
  handleArduinoData();
  
  // Update system data
  updateSystemData();
  
  // Send periodic updates to connected clients
  if (millis() - lastBroadcast > 1000) {
    broadcastGameData();
    lastBroadcast = millis();
  }
  
  delay(10);
}

void setupWebServer() {
  // Serve the main HTML file
  server.on("/", HTTP_GET, []() {
    File file = SPIFFS.open("/index.html", "r");
    if (file) {
      server.streamFile(file, "text/html");
      file.close();
    } else {
      server.send(404, "text/plain", "File not found. Please upload web files to SPIFFS.");
    }
  });
  
  // Serve CSS file
  server.on("/styles.css", HTTP_GET, []() {
    File file = SPIFFS.open("/styles.css", "r");
    if (file) {
      server.streamFile(file, "text/css");
      file.close();
    } else {
      server.send(404, "text/plain", "CSS file not found");
    }
  });
  
  // Serve JavaScript file
  server.on("/app.js", HTTP_GET, []() {
    File file = SPIFFS.open("/app.js", "r");
    if (file) {
      server.streamFile(file, "application/javascript");
      file.close();
    } else {
      server.send(404, "text/plain", "JS file not found");
    }
  });
  
  // API endpoints
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/gamedata", HTTP_GET, handleGameData);
  server.on("/api/highscores", HTTP_GET, handleHighScores);
  server.on("/api/control", HTTP_POST, handleControl);
  server.on("/api/register", HTTP_POST, handleRegister);
  server.on("/api/login", HTTP_POST, handleLogin);
  
  // Enable CORS for all routes
  server.enableCORS(true);
  
  // Handle file not found
  server.onNotFound([]() {
    server.send(404, "text/plain", "File not found");
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
        // Forward LED command to Arduino
        String ledCommand = "LED:";
        ledCommand += String(gameData.ledR) + ",";
        ledCommand += String(gameData.ledG) + ",";
        ledCommand += String(gameData.ledB) + ",";
        ledCommand += String(gameData.ledBrightness) + ",";
        ledCommand += gameData.ledMode + ",";
        ledCommand += String(gameData.ledOn ? 1 : 0) + ",";
        ledCommand += String(gameData.ledGameSync ? 1 : 0);
        Serial1.println(ledCommand); // Forward to Arduino
      } else if (doc["type"] == "CONTROL") {
        String action = doc["action"];
        String ctrlCommand = "CONTROL:" + action;
        Serial1.println(ctrlCommand); // Forward to Arduino
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
  
  // Send LED command to Arduino - Fixed string concatenation
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
    gameData.snakeHigh = values[4].toInt();  // Fixed: now uses snakeHigh
    gameData.pongHigh = values[5].toInt();
    
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
    Serial.println("High scores loaded from SPIFFS");
  } else {
    // Create some default high scores
    addHighScore("Player1", "Flappy Bird", 15);
    addHighScore("Player2", "Snake", 8);
    addHighScore("Player3", "Pong", 12);
    Serial.println("Created default high scores");
  }
}

void addHighScore(String player, String game, int score) {
  if (highScoreCount < 20) {
    highScores[highScoreCount].player = player;
    highScores[highScoreCount].game = game;
    highScores[highScoreCount].score = score;
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
  }
}

// Helper: SHA256 hash using BearSSL
String sha256(const String& input) {
  br_sha256_context ctx;
  br_sha256_init(&ctx);
  br_sha256_update(&ctx, (const unsigned char*)input.c_str(), input.length());
  unsigned char hash[32];
  br_sha256_out(&ctx, hash);
  String hex = "";
  for (int i = 0; i < 32; i++) {
    if (hash[i] < 16) hex += "0";
    hex += String(hash[i], HEX);
  }
  return hex;
}

// Helper: Generate random token
String generateToken() {
  String token = "";
  for (int i = 0; i < 32; i++) token += String(random(0, 16), HEX);
  return token;
}

// Load users from SPIFFS
void loadUsers() {
  users.clear();
  File file = SPIFFS.open("/users.json", "r");
  if (file) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, file);
    JsonArray arr = doc["users"].as<JsonArray>();
    for (JsonObject u : arr) {
      User user;
      user.username = u["username"].as<String>();
      user.passwordHash = u["passwordHash"].as<String>();
      users.push_back(user);
    }
    file.close();
  }
}

// Save users to SPIFFS
void saveUsers() {
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.createNestedArray("users");
  for (User& u : users) {
    JsonObject obj = arr.createNestedObject();
    obj["username"] = u.username;
    obj["passwordHash"] = u.passwordHash;
  }
  File file = SPIFFS.open("/users.json", "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
}

// Find user by username
User* findUser(const String& username) {
  for (auto& u : users) {
    if (u.username == username) return &u;
  }
  return nullptr;
}

// API: Register
void handleRegister() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data\"}");
    return;
  }
  DynamicJsonDocument doc(256);
  deserializeJson(doc, server.arg("plain"));
  String username = doc["username"];
  String password = doc["password"];
  if (username.length() < 3 || password.length() < 4) {
    server.send(400, "application/json", "{\"error\":\"Username or password too short\"}");
    return;
  }
  if (findUser(username)) {
    server.send(409, "application/json", "{\"error\":\"User exists\"}");
    return;
  }
  User user;
  user.username = username;
  user.passwordHash = sha256(password);
  users.push_back(user);
  saveUsers();
  server.send(200, "application/json", "{\"status\":\"registered\"}");
}

// API: Login
void handleLogin() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"No data\"}");
    return;
  }
  DynamicJsonDocument doc(256);
  deserializeJson(doc, server.arg("plain"));
  String username = doc["username"];
  String password = doc["password"];
  User* user = findUser(username);
  if (!user || user->passwordHash != sha256(password)) {
    server.send(401, "application/json", "{\"error\":\"Invalid credentials\"}");
    return;
  }
  String token = generateToken();
  sessions[token] = username;
  DynamicJsonDocument resp(128);
  resp["token"] = token;
  String out;
  serializeJson(resp, out);
  server.send(200, "application/json", out);
}

// Helper: Validate token
String validateToken() {
  if (!server.hasHeader("Authorization")) return "";
  String token = server.header("Authorization");
  if (sessions.count(token)) return sessions[token];
  return "";
}