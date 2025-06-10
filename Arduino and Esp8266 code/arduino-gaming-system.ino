/*
 * Arduino Gaming System - Main Controller
 * Hardware: Arduino Uno/Nano, 16x2 LCD, 4x Max7219 LED Matrix, 5-button joystick, buzzer
 * Communication: Serial to ESP8266 for web dashboard data
 */

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>
#include <LiquidCrystal.h>
#include <EEPROM.h>

// ===== HARDWARE CONFIGURATION =====
// LED Matrix
const int pinCS = 10;
const int numberOfHorizontalDisplays = 4;
const int numberOfVerticalDisplays = 1;

// LCD (16x2)
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Control pins
const int up = 6;
const int down = 7;
const int left = 8;
const int right = 9;
const int trigger = A0;
const int buzzerPin = A1;

// Display setup
Max72xxPanel matrix = Max72xxPanel(pinCS, numberOfHorizontalDisplays, numberOfVerticalDisplays);

// LED Control Pins
const int ledRedPin = A2;
const int ledGreenPin = A3;
const int ledBluePin = A4;

// LED Control Variables
struct LEDState {
  int r, g, b;
  int brightness;
  String mode;
  bool isOn;
} ledState = {0, 0, 0, 100, "solid", false};

// LED State Persistence
const int EEPROM_LED_STATE_ADDR = 512; // After game stats

// Game Event LED Effects
enum GameEvent {
  EVENT_NONE,
  EVENT_GAME_START,
  EVENT_GAME_OVER,
  EVENT_LEVEL_UP,
  EVENT_BOSS_BATTLE,
  EVENT_HIGH_SCORE,
  EVENT_POWER_UP
};

GameEvent currentEvent = EVENT_NONE;
unsigned long eventStartTime = 0;
const unsigned long EVENT_DURATION = 2000; // 2 seconds

// ===== SERIAL COMMUNICATION =====
String serialBuffer = "";
String lastWebCommand = "";
unsigned long lastWebCommandTime = 0;
unsigned long lastDataSend = 0;
const unsigned long dataSendInterval = 1000; // Send data every second

// ===== GAME STATE MANAGEMENT =====
enum GameState {
  MAIN_MENU,
  GAME_MENU,
  UFO_ATTACK,
  SNAKE_GAME,
  SETTINGS,
  HIGH_SCORES
};

GameState currentState = MAIN_MENU;
int menuSelection = 0;
int maxMenuItems = 4;
unsigned long lastInputTime = 0;
const unsigned long inputDelay = 200;

// ===== MENU SYSTEM =====
struct MenuItem {
  String name;
  String description;
};

MenuItem mainMenu[] = {
  {"Play Games", "Start gaming"},
  {"High Scores", "View records"},
  {"Settings", "Game options"},
  {"About", "System info"}
};

MenuItem gameMenu[] = {
  {"UFO Attack", "Space shooter"},
  {"Snake Game", "Classic arcade"},
  {"Back", "Return to main"}
};

// ===== HIGH SCORE SYSTEM =====
struct HighScore {
  char playerName[10];
  int score;
  char game[10];
  unsigned long timestamp;
};

HighScore highScores[10];

// ===== SPRITE DEFINITIONS =====
const unsigned char ship[] PROGMEM = {
  0x80, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char ufo[] PROGMEM = {
  0x40, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char explosion[] PROGMEM = {
  0xe0, 0xe0, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00
};

const unsigned char boss[] PROGMEM = {
  0x18, 0x24, 0x42, 0xff, 0x24, 0xff, 0x7e, 0x3c
};

// ===== UFO ATTACK VARIABLES =====
bool ufoGameActive = false;
bool ufoGameStart = false;

int shipx = 0, shipy = 3;
int lasx, lasy;
bool lasExist = false;
int lives = 5;
int ufoScore = 0;
int levelNumber = 1;
int kills = 0, killsTarget = 10;

struct Enemy {
  int x, y;
  bool exist;
  int health;
};

Enemy ufos[3];
Enemy bossEnemy;
bool bossBattleStarted = false;
int bossHealth = 10;

unsigned long lastGameUpdate = 0;
const unsigned long gameUpdateInterval = 50;
unsigned long spawnTimer = 0;

// ===== SNAKE GAME VARIABLES =====
bool snakeGameActive = false;
bool snakeGameStart = false;

const int maxSnakeLength = 15;
int snakeLength = 2;
int snake[maxSnakeLength][2];
int snakeDirection = 0;
int food[2] = {0, 0};
bool foodEaten = true;
int snakeScore = 0;

unsigned long lastSnakeUpdate = 0;
unsigned long snakeUpdateInterval = 500;
unsigned long lastSnakeInput = 0;
const unsigned long snakeInputDelay = 100;

byte snakeScene[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// ===== STATISTICS =====
struct GameStats {
  unsigned long totalGamesPlayed;
  unsigned long totalPlayTime;
  unsigned long sessionStartTime;
  int bestUFOScore;
  int bestSnakeScore;
};

GameStats gameStats;

// ===== SETUP =====
void setup() {
  Serial.begin(9600);
  
  // Initialize EEPROM
  loadHighScores();
  loadGameStats();
  
  // Initialize pins
  pinMode(up, INPUT_PULLUP);
  pinMode(down, INPUT_PULLUP);
  pinMode(left, INPUT_PULLUP);
  pinMode(right, INPUT_PULLUP);
  pinMode(trigger, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  
  // Initialize LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("ARCADE SYSTEM");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  // Initialize LED Matrix
  matrix.setIntensity(2);
  matrix.setPosition(0, 3, 0);
  matrix.setPosition(1, 2, 0);
  matrix.setPosition(2, 1, 0);
  matrix.setPosition(3, 0, 0);
  matrix.setRotation(0, 3);
  matrix.setRotation(1, 3);
  matrix.setRotation(2, 3);
  matrix.setRotation(3, 3);
  matrix.fillScreen(LOW);
  
  // Welcome animation
  welcomeAnimation();
  
  currentState = MAIN_MENU;
  maxMenuItems = 4;
  menuSelection = 0;
  
  updateLCDMenu();
  
  // Send ready signal to ESP8266
  Serial.println("ARDUINO_READY");
  
  setupLED();
  loadLEDState();
}

// ===== MAIN LOOP =====
void loop() {
  // Handle serial communication with ESP8266
  handleSerialCommunication();
  
  // Handle web commands
  handleWebCommands();
  
  // Main game state machine
  switch (currentState) {
    case MAIN_MENU:
      handleMainMenu();
      break;
    case GAME_MENU:
      handleGameMenu();
      break;
    case UFO_ATTACK:
      runUFOAttack();
      break;
    case SNAKE_GAME:
      runSnakeGame();
      break;
    case SETTINGS:
      handleSettings();
      break;
    case HIGH_SCORES:
      handleHighScores();
      break;
  }
  
  // Send data to ESP8266 periodically
  if (millis() - lastDataSend > dataSendInterval) {
    sendGameDataToESP();
    lastDataSend = millis();
  }
  
  // Update statistics
  updateGameStats();
  
  // Update LED effects
  updateLED();
}

// ===== SERIAL COMMUNICATION =====
void handleSerialCommunication() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      processSerialCommand(serialBuffer);
      serialBuffer = "";
    } else {
      serialBuffer += c;
    }
  }
}

void processSerialCommand(String command) {
  command.trim();
  
  if (command.startsWith("CMD:")) {
    // Web command from ESP8266
    lastWebCommand = command.substring(4);
    lastWebCommandTime = millis();
  }
  else if (command.startsWith("LED_CONTROL:")) {
    handleLEDCommand(command);
  }
  else if (command == "GET_DATA") {
    // ESP8266 requesting current game data
    sendGameDataToESP();
  }
  else if (command == "GET_SCORES") {
    // ESP8266 requesting high scores
    sendHighScoresToESP();
  }
  else if (command == "GET_STATS") {
    // ESP8266 requesting statistics
    sendStatsToESP();
  }
}

void sendGameDataToESP() {
  // Send current game data in JSON-like format
  Serial.print("DATA:");
  Serial.print("{");
  Serial.print("\"ufoScore\":" + String(ufoScore) + ",");
  Serial.print("\"ufoLevel\":" + String(levelNumber) + ",");
  Serial.print("\"ufoLives\":" + String(lives) + ",");
  Serial.print("\"ufoKills\":" + String(kills) + ",");
  Serial.print("\"snakeScore\":" + String(snakeScore) + ",");
  Serial.print("\"snakeLength\":" + String(snakeLength) + ",");
  Serial.print("\"snakeSpeed\":" + String(max(1, 6 - (snakeUpdateInterval / 100))) + ",");
  Serial.print("\"snakeFood\":" + String(foodEaten ? 0 : 1) + ",");
  
  // Current game state
  String currentGame = "menu";
  if (ufoGameActive) currentGame = "ufo";
  else if (snakeGameActive) currentGame = "snake";
  Serial.print("\"currentGame\":\"" + currentGame + "\",");
  
  // Game status
  Serial.print("\"ufoActive\":" + String(ufoGameActive ? "true" : "false") + ",");
  Serial.print("\"snakeActive\":" + String(snakeGameActive ? "true" : "false") + ",");
  Serial.print("\"bossBattle\":" + String(bossBattleStarted ? "true" : "false") + ",");
  Serial.print("\"uptime\":" + String(millis()));
  Serial.println("}");
}

void sendHighScoresToESP() {
  Serial.print("SCORES:[");
  bool first = true;
  for (int i = 0; i < 10; i++) {
    if (highScores[i].score > 0) {
      if (!first) Serial.print(",");
      Serial.print("{");
      Serial.print("\"player\":\"" + String(highScores[i].playerName) + "\",");
      Serial.print("\"game\":\"" + String(highScores[i].game) + "\",");
      Serial.print("\"score\":" + String(highScores[i].score) + ",");
      Serial.print("\"timestamp\":" + String(highScores[i].timestamp));
      Serial.print("}");
      first = false;
    }
  }
  Serial.println("]");
}

void sendStatsToESP() {
  Serial.print("STATS:");
  Serial.print("{");
  Serial.print("\"totalGames\":" + String(gameStats.totalGamesPlayed) + ",");
  Serial.print("\"totalPlayTime\":" + String(gameStats.totalPlayTime) + ",");
  Serial.print("\"bestUFOScore\":" + String(gameStats.bestUFOScore) + ",");
  Serial.print("\"bestSnakeScore\":" + String(gameStats.bestSnakeScore) + ",");
  Serial.print("\"currentSession\":" + String(millis() - gameStats.sessionStartTime));
  Serial.println("}");
}

// ===== WEB COMMAND HANDLING =====
void handleWebCommands() {
  // Timeout web commands after 100ms
  if (millis() - lastWebCommandTime > 100) {
    lastWebCommand = "";
    return;
  }
  
  if (lastWebCommand == "") return;
  
  // Process web command as if it were a button press
  if (lastWebCommand == "up") {
    processInput(up, false);
  } else if (lastWebCommand == "down") {
    processInput(down, false);
  } else if (lastWebCommand == "left") {
    processInput(left, false);
  } else if (lastWebCommand == "right") {
    processInput(right, false);
  } else if (lastWebCommand == "fire") {
    processInput(trigger, false);
  } else if (lastWebCommand == "menu") {
    // Special menu command - return to main menu
    if (currentState != MAIN_MENU) {
      returnToMenu();
    }
  }
  
  // Clear command after processing
  lastWebCommand = "";
}

void processInput(int pin, bool isPhysical) {
  // Unified input processing for both physical and web controls
  if (isPhysical) {
    // For physical buttons, check the actual pin state
    if (digitalRead(pin) == HIGH) return; // Button not pressed
  }
  
  // Process the input based on current state
  switch (currentState) {
    case MAIN_MENU:
      handleMenuInput(pin);
      break;
    case GAME_MENU:
      handleGameMenuInput(pin);
      break;
    case UFO_ATTACK:
      handleUFOInput(pin);
      break;
    case SNAKE_GAME:
      handleSnakeInputPin(pin);
      break;
    default:
      handleGenericInput(pin);
      break;
  }
}

// ===== INPUT HANDLING FUNCTIONS =====
void handleMenuInput(int pin) {
  if (millis() - lastInputTime < inputDelay) return;
  
  if (pin == up) {
    menuSelection = (menuSelection - 1 + maxMenuItems) % maxMenuItems;
    updateLCDMenu();
    tone(buzzerPin, 600, 50);
    lastInputTime = millis();
  }
  else if (pin == down) {
    menuSelection = (menuSelection + 1) % maxMenuItems;
    updateLCDMenu();
    tone(buzzerPin, 600, 50);
    lastInputTime = millis();
  }
  else if (pin == trigger) {
    selectMainMenuItem();
    lastInputTime = millis();
  }
}

void handleGameMenuInput(int pin) {
  if (millis() - lastInputTime < inputDelay) return;
  
  if (pin == up) {
    menuSelection = (menuSelection - 1 + 3) % 3;
    updateLCDMenu();
    tone(buzzerPin, 600, 50);
    lastInputTime = millis();
  }
  else if (pin == down) {
    menuSelection = (menuSelection + 1) % 3;
    updateLCDMenu();
    tone(buzzerPin, 600, 50);
    lastInputTime = millis();
  }
  else if (pin == trigger) {
    selectGameMenuItem();
    lastInputTime = millis();
  }
}

void handleUFOInput(int pin) {
  if (pin == up && shipy > 0) shipy--;
  if (pin == down && shipy < 6) shipy++;
  if (pin == left && shipx > 0) shipx--;
  if (pin == right && shipx < 29) shipx++;
  
  if (pin == trigger && !lasExist) {
    lasx = shipx + 3;
    lasy = shipy + 1;
    lasExist = true;
    tone(buzzerPin, 400, 75);
  }
}

void handleSnakeInputPin(int pin) {
  if (millis() - lastSnakeInput < snakeInputDelay) return;
  
  int newDirection = snakeDirection;
  
  if (pin == left) {
    newDirection = (snakeDirection + 3) % 4;
  }
  else if (pin == right) {
    newDirection = (snakeDirection + 1) % 4;
  }
  
  if ((newDirection + 2) % 4 != snakeDirection) {
    snakeDirection = newDirection;
    lastSnakeInput = millis();
    tone(buzzerPin, 800, 30);
  }
}

void handleGenericInput(int pin) {
  if (pin == trigger) {
    returnToMenu();
  }
}

// ===== MENU FUNCTIONS =====
void welcomeAnimation() {
  lcd.clear();
  lcd.print("ARCADE SYSTEM");
  lcd.setCursor(0, 1);
  lcd.print("v2.0 - Arduino");
  
  // LED Matrix animation
  for (int i = 0; i < 32; i++) {
    matrix.fillScreen(LOW);
    matrix.drawLine(i, 0, i, 7, 1);
    matrix.write();
    delay(50);
  }
  
  // Startup sound
  for (int i = 0; i < 3; i++) {
    tone(buzzerPin, 800 + i * 200, 150);
    delay(200);
  }
  
  delay(1000);
}

void updateLCDMenu() {
  lcd.clear();
  
  switch (currentState) {
    case MAIN_MENU:
      lcd.print("MAIN MENU");
      lcd.setCursor(0, 1);
      lcd.print(">" + mainMenu[menuSelection].name);
      break;
      
    case GAME_MENU:
      lcd.print("SELECT GAME");
      lcd.setCursor(0, 1);
      lcd.print(">" + gameMenu[menuSelection].name);
      break;
      
    case HIGH_SCORES:
      lcd.print("HIGH SCORES");
      lcd.setCursor(0, 1);
      if (highScores[menuSelection].score > 0) {
        lcd.print(String(highScores[menuSelection].playerName) + ":" + String(highScores[menuSelection].score));
      } else {
        lcd.print("No scores yet");
      }
      break;
      
    case SETTINGS:
      lcd.print("SETTINGS");
      lcd.setCursor(0, 1);
      lcd.print("Press FIRE=Back");
      break;
  }
}

void handleMainMenu() {
  if (digitalRead(up) == LOW) processInput(up, true);
  else if (digitalRead(down) == LOW) processInput(down, true);
  else if (digitalRead(trigger) == LOW) processInput(trigger, true);
}

void handleGameMenu() {
  if (digitalRead(up) == LOW) processInput(up, true);
  else if (digitalRead(down) == LOW) processInput(down, true);
  else if (digitalRead(trigger) == LOW) processInput(trigger, true);
}

void selectMainMenuItem() {
  tone(buzzerPin, 1000, 100);
  
  switch (menuSelection) {
    case 0: // Play Games
      currentState = GAME_MENU;
      maxMenuItems = 3;
      menuSelection = 0;
      break;
    case 1: // High Scores
      currentState = HIGH_SCORES;
      maxMenuItems = 10;
      menuSelection = 0;
      break;
    case 2: // Settings
      currentState = SETTINGS;
      break;
    case 3: // About
      lcd.clear();
      lcd.print("Arduino Gaming");
      lcd.setCursor(0, 1);
      lcd.print("System v2.0");
      delay(2000);
      break;
  }
  updateLCDMenu();
}

void selectGameMenuItem() {
  tone(buzzerPin, 1000, 100);
  
  switch (menuSelection) {
    case 0: // UFO Attack
      currentState = UFO_ATTACK;
      initUFOAttack();
      gameStats.totalGamesPlayed++;
      gameStats.sessionStartTime = millis();
      break;
    case 1: // Snake Game
      currentState = SNAKE_GAME;
      initSnakeGame();
      gameStats.totalGamesPlayed++;
      gameStats.sessionStartTime = millis();
      break;
    case 2: // Back
      currentState = MAIN_MENU;
      maxMenuItems = 4;
      menuSelection = 0;
      updateLCDMenu();
      break;
  }
}

void handleSettings() {
  if (digitalRead(trigger) == LOW) {
    currentState = MAIN_MENU;
    menuSelection = 0;
    updateLCDMenu();
    delay(200);
  }
}

void handleHighScores() {
  if (millis() - lastInputTime > inputDelay) {
    if (digitalRead(up) == LOW) {
      menuSelection = (menuSelection - 1 + 10) % 10;
      updateLCDMenu();
      tone(buzzerPin, 600, 50);
      lastInputTime = millis();
    }
    else if (digitalRead(down) == LOW) {
      menuSelection = (menuSelection + 1) % 10;
      updateLCDMenu();
      tone(buzzerPin, 600, 50);
      lastInputTime = millis();
    }
    else if (digitalRead(trigger) == LOW) {
      currentState = MAIN_MENU;
      menuSelection = 0;
      updateLCDMenu();
      lastInputTime = millis();
    }
  }
}

void returnToMenu() {
  currentState = MAIN_MENU;
  menuSelection = 0;
  updateLCDMenu();
  matrix.fillScreen(LOW);
  tone(buzzerPin, 400, 200);
  delay(500);
}

// ===== HIGH SCORE SYSTEM =====
void loadHighScores() {
  for (int i = 0; i < 10; i++) {
    EEPROM.get(i * sizeof(HighScore), highScores[i]);
    if (highScores[i].score < 0 || highScores[i].score > 999999) {
      // Initialize if corrupted
      strcpy(highScores[i].playerName, "PLAYER");
      highScores[i].score = 0;
      strcpy(highScores[i].game, "NONE");
      highScores[i].timestamp = 0;
    }
  }
}

void saveHighScores() {
  for (int i = 0; i < 10; i++) {
    EEPROM.put(i * sizeof(HighScore), highScores[i]);
  }
}

void addHighScore(String playerName, int score, String gameName) {
  // Find insertion point
  int insertPos = -1;
  for (int i = 0; i < 10; i++) {
    if (score > highScores[i].score) {
      insertPos = i;
      break;
    }
  }
  
  if (insertPos >= 0) {
    // Shift scores down
    for (int i = 9; i > insertPos; i--) {
      highScores[i] = highScores[i-1];
    }
    
    // Insert new score
    playerName.toCharArray(highScores[insertPos].playerName, 10);
    highScores[insertPos].score = score;
    gameName.toCharArray(highScores[insertPos].game, 10);
    highScores[insertPos].timestamp = millis();
    
    saveHighScores();
  }
}

void updateGameStats() {
  static unsigned long lastStatsUpdate = 0;
  if (millis() - lastStatsUpdate < 10000) return; // Update every 10 seconds
  
  lastStatsUpdate = millis();
  
  // Update best scores
  if (ufoScore > gameStats.bestUFOScore) {
    gameStats.bestUFOScore = ufoScore;
  }
  if (snakeScore > gameStats.bestSnakeScore) {
    gameStats.bestSnakeScore = snakeScore;
  }
  
  // Update play time
  if (ufoGameActive || snakeGameActive) {
    gameStats.totalPlayTime += 10; // Add 10 seconds
  }
  
  saveGameStats();
}

void loadGameStats() {
  EEPROM.get(sizeof(HighScore) * 10, gameStats);
  
  // Initialize if corrupted
  if (gameStats.totalGamesPlayed > 999999) {
    gameStats.totalGamesPlayed = 0;
    gameStats.totalPlayTime = 0;
    gameStats.bestUFOScore = 0;
    gameStats.bestSnakeScore = 0;
    gameStats.sessionStartTime = millis();
  }
}

void saveGameStats() {
  EEPROM.put(sizeof(HighScore) * 10, gameStats);
}

// ===== UFO ATTACK GAME =====
void initUFOAttack() {
  ufoGameActive = true;
  ufoGameStart = false;
  
  shipx = 0; shipy = 3;
  lives = 5; ufoScore = 0; levelNumber = 1;
  kills = 0; killsTarget = 10;
  lasExist = false;
  bossBattleStarted = false;
  bossHealth = 10;
  
  for (int i = 0; i < 3; i++) {
    ufos[i].exist = false;
    ufos[i].x = 34;
    ufos[i].y = random(0, 7);
  }
  
  bossEnemy.exist = false;
  bossEnemy.x = 34;
  bossEnemy.y = 0;
  
  lcd.clear();
  lcd.print("UFO ATTACK!");
  lcd.setCursor(0, 1);
  lcd.print("Press FIRE start");
  
  triggerGameEvent(EVENT_GAME_START);
}

void runUFOAttack() {
  if (!ufoGameActive) return;
  
  if (!ufoGameStart) {
    if (digitalRead(trigger) == LOW || lastWebCommand == "fire") {
      ufoGameStart = true;
      spawnTimer = millis();
      tone(buzzerPin, 800, 100);
      delay(200);
    }
    return;
  }
  
  // Update LCD with game info
  lcd.clear();
  lcd.print("L:" + String(levelNumber) + " S:" + String(ufoScore));
  lcd.setCursor(0, 1);
  lcd.print("Lives:" + String(lives) + " K:" + String(kills));
  
  // Check for exit to menu
  if ((digitalRead(up) == LOW && digitalRead(down) == LOW) || lastWebCommand == "menu") {
    ufoGameActive = false;
    returnToMenu();
    return;
  }
  
  if (millis() - lastGameUpdate >= gameUpdateInterval) {
    lastGameUpdate = millis();
    
    matrix.fillScreen(LOW);
    
    // Handle both physical and web inputs
    if (digitalRead(up) == LOW) processInput(up, true);
    if (digitalRead(down) == LOW) processInput(down, true);
    if (digitalRead(left) == LOW) processInput(left, true);
    if (digitalRead(right) == LOW) processInput(right, true);
    if (digitalRead(trigger) == LOW) processInput(trigger, true);
    
    updateUFOEnemies();
    updateUFOLasers();
    checkUFOCollisions();
    drawUFOGame();
    
    matrix.write();
    
    if (lives <= 0) {
      gameOverUFO();
    }
  }
}

void updateUFOEnemies() {
  if (millis() - spawnTimer > 3000) {
    spawnUFO();
    spawnTimer = millis();
  }
  
  for (int i = 0; i < 3; i++) {
    if (ufos[i].exist) {
      ufos[i].x--;
      if (ufos[i].x < -3) {
        ufos[i].x = 33;
        ufos[i].y = random(0, 7);
      }
    }
  }
  
  if (kills >= killsTarget && !bossBattleStarted) {
    startBossBattle();
  }
  
  if (bossEnemy.exist) {
    updateBoss();
  }
}

void spawnUFO() {
  for (int i = 0; i < 3; i++) {
    if (!ufos[i].exist) {
      ufos[i].exist = true;
      ufos[i].x = 32;
      ufos[i].y = random(0, 7);
      break;
    }
  }
}

void startBossBattle() {
  bossBattleStarted = true;
  bossEnemy.exist = true;
  bossEnemy.x = 34;
  bossEnemy.y = 0;
  bossEnemy.health = bossHealth;
  
  lcd.clear();
  lcd.print("BOSS BATTLE!");
  lcd.setCursor(0, 1);
  lcd.print("Health:" + String(bossEnemy.health));
  
  for (int i = 0; i < 5; i++) {
    tone(buzzerPin, 200 + i * 100, 200);
    delay(250);
  }
  
  triggerGameEvent(EVENT_BOSS_BATTLE);
}

void updateBoss() {
  if (bossEnemy.x > 24) {
    bossEnemy.x--;
  }
}

void updateUFOLasers() {
  if (lasExist) {
    lasx++;
    if (lasx > 31) {
      lasExist = false;
    }
  }
}

void checkUFOCollisions() {
  if (!lasExist) return;
  
  for (int i = 0; i < 3; i++) {
    if (ufos[i].exist) {
      if (lasx >= ufos[i].x && lasx <= ufos[i].x + 2 &&
          lasy >= ufos[i].y && lasy <= ufos[i].y + 1) {
        ufos[i].exist = false;
        lasExist = false;
        kills++;
        ufoScore += 10;
        explodeAt(ufos[i].x, ufos[i].y);
      }
    }
  }
  
  if (bossEnemy.exist) {
    if (lasx >= bossEnemy.x && lasx <= bossEnemy.x + 7 &&
        lasy >= bossEnemy.y && lasy <= bossEnemy.y + 7) {
      bossEnemy.health--;
      lasExist = false;
      explodeAt(lasx, lasy);
      
      if (bossEnemy.health <= 0) {
        bossEnemy.exist = false;
        bossBattleStarted = false;
        ufoScore += 100 * levelNumber;
        levelNumber++;
        kills = 0;
        killsTarget += 10;
        
        for (int i = 0; i < 3; i++) {
          tone(buzzerPin, 800 + i * 200, 200);
          delay(250);
        }
      }
    }
  }
}

void explodeAt(int x, int y) {
  tone(buzzerPin, 75, 75);
  matrix.drawBitmap(x, y, explosion, 3, 3, 1);
}

void drawUFOGame() {
  matrix.drawBitmap(shipx, shipy, ship, 3, 2, 1);
  
  if (lasExist) {
    matrix.drawPixel(lasx, lasy, 1);
  }
  
  for (int i = 0; i < 3; i++) {
    if (ufos[i].exist) {
      matrix.drawBitmap(ufos[i].x, ufos[i].y, ufo, 3, 2, 1);
    }
  }
  
  if (bossEnemy.exist) {
    matrix.drawBitmap(bossEnemy.x, bossEnemy.y, boss, 8, 8, 1);
  }
  
  for (int i = 0; i < lives && i < 5; i++) {
    matrix.drawPixel(i, 7, 1);
  }
}

void gameOverUFO() {
  lcd.clear();
  lcd.print("GAME OVER!");
  lcd.setCursor(0, 1);
  lcd.print("Score: " + String(ufoScore));
  
  for (int i = 0; i < 5; i++) {
    matrix.fillScreen(HIGH);
    matrix.write();
    tone(buzzerPin, 200, 200);
    delay(300);
    matrix.fillScreen(LOW);
    matrix.write();
    delay(300);
  }
  
  addHighScore("PLAYER", ufoScore, "UFO");
  
  delay(3000);
  ufoGameActive = false;
  returnToMenu();
  
  triggerGameEvent(EVENT_GAME_OVER);
}

// ===== SNAKE GAME =====
void initSnakeGame() {
  snakeGameActive = true;
  snakeGameStart = false;
  
  snakeLength = 2;
  snakeDirection = 0;
  foodEaten = true;
  snakeScore = 0;
  snakeUpdateInterval = 500;
  
  for (int i = 0; i < maxSnakeLength; i++) {
    snake[i][0] = 0;
    snake[i][1] = 0;
  }
  
  snake[maxSnakeLength - 1][0] = 3;
  snake[maxSnakeLength - 1][1] = 4;
  snake[maxSnakeLength - 2][0] = 2;
  snake[maxSnakeLength - 2][1] = 4;
  
  lcd.clear();
  lcd.print("SNAKE GAME");
  lcd.setCursor(0, 1);
  lcd.print("Press FIRE start");
  
  triggerGameEvent(EVENT_GAME_START);
}

void runSnakeGame() {
  if (!snakeGameActive) return;
  
  if (!snakeGameStart) {
    if (digitalRead(trigger) == LOW || lastWebCommand == "fire") {
      snakeGameStart = true;
      tone(buzzerPin, 600, 100);
      delay(200);
    }
    return;
  }
  
  lcd.clear();
  lcd.print("Score: " + String(snakeScore));
  lcd.setCursor(0, 1);
  lcd.print("Length: " + String(snakeLength));
  
  if ((digitalRead(up) == LOW && digitalRead(down) == LOW) || lastWebCommand == "menu") {
    snakeGameActive = false;
    returnToMenu();
    return;
  }
  
  // Handle both physical and web inputs
  if (digitalRead(left) == LOW) processInput(left, true);
  if (digitalRead(right) == LOW) processInput(right, true);
  
  if (millis() - lastSnakeUpdate >= snakeUpdateInterval) {
    lastSnakeUpdate = millis();
    
    updateSnake();
    checkSnakeCollisions();
    drawSnakeGame();
    
    snakeUpdateInterval = max(100, 500 - (snakeScore * 10));
  }
}

void updateSnake() {
  if (foodEaten) {
    generateFood();
    foodEaten = false;
  }
  
  if (snake[maxSnakeLength - 1][0] == food[0] && 
      snake[maxSnakeLength - 1][1] == food[1]) {
    foodEaten = true;
    snakeLength++;
    snakeScore++;
    tone(buzzerPin, 1000, 100);
  }
  
  for (int i = snakeLength - 1; i >= 1; i--) {
    snake[maxSnakeLength - 1 - i][0] = snake[maxSnakeLength - i][0];
    snake[maxSnakeLength - 1 - i][1] = snake[maxSnakeLength - i][1];
  }
  
  switch (snakeDirection) {
    case 0: snake[maxSnakeLength - 1][0]++; break;
    case 1: snake[maxSnakeLength - 1][1]--; break;
    case 2: snake[maxSnakeLength - 1][0]--; break;
    case 3: snake[maxSnakeLength - 1][1]++; break;
  }
  
  if (snake[maxSnakeLength - 1][0] < 0) snake[maxSnakeLength - 1][0] = 7;
  if (snake[maxSnakeLength - 1][0] > 7) snake[maxSnakeLength - 1][0] = 0;
  if (snake[maxSnakeLength - 1][1] < 0) snake[maxSnakeLength - 1][1] = 7;
  if (snake[maxSnakeLength - 1][1] > 7) snake[maxSnakeLength - 1][1] = 0;
}

void generateFood() {
  bool validPosition = false;
  
  while (!validPosition) {
    food[0] = random(0, 8);
    food[1] = random(0, 8);
    
    validPosition = true;
    for (int i = maxSnakeLength - snakeLength; i < maxSnakeLength; i++) {
      if (snake[i][0] == food[0] && snake[i][1] == food[1]) {
        validPosition = false;
        break;
      }
    }
  }
}

void checkSnakeCollisions() {
  for (int i = maxSnakeLength - snakeLength; i < maxSnakeLength - 1; i++) {
    if (snake[i][0] == snake[maxSnakeLength - 1][0] && 
        snake[i][1] == snake[maxSnakeLength - 1][1]) {
      gameOverSnake();
      return;
    }
  }
  
  if (snakeLength >= maxSnakeLength) {
    winSnake();
  }
}

void drawSnakeGame() {
  for (int i = 0; i < 8; i++) {
    snakeScene[i] = 0x00;
  }
  
  for (int i = maxSnakeLength - snakeLength; i < maxSnakeLength; i++) {
    if (snake[i][0] >= 0 && snake[i][0] < 8 && 
        snake[i][1] >= 0 && snake[i][1] < 8) {
      snakeScene[snake[i][0]] |= (1 << snake[i][1]);
    }
  }
  
  if (!foodEaten && food[0] >= 0 && food[0] < 8 && 
      food[1] >= 0 && food[1] < 8) {
    snakeScene[food[0]] |= (1 << food[1]);
  }
  
  matrix.fillScreen(LOW);
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (snakeScene[row] & (1 << col)) {
        matrix.drawPixel(row, col, 1);
      }
    }
  }
  matrix.write();
}

void gameOverSnake() {
  lcd.clear();
  lcd.print("GAME OVER!");
  lcd.setCursor(0, 1);
  lcd.print("Score: " + String(snakeScore));
  
  for (int i = 0; i < 4; i++) {
    matrix.fillScreen(HIGH);
    matrix.write();
    tone(buzzerPin, 200, 300);
    delay(400);
    matrix.fillScreen(LOW);
    matrix.write();
    delay(400);
  }
  
  addHighScore("PLAYER", snakeScore, "SNAKE");
  
  delay(3000);
  snakeGameActive = false;
  returnToMenu();
  
  triggerGameEvent(EVENT_GAME_OVER);
}

void winSnake() {
  lcd.clear();
  lcd.print("YOU WIN!");
  lcd.setCursor(0, 1);
  lcd.print("Perfect Score!");
  
  for (int i = 0; i < 3; i++) {
    matrix.fillScreen(HIGH);
    matrix.write();
    tone(buzzerPin, 800 + i * 200, 300);
    delay(400);
    matrix.fillScreen(LOW);
    matrix.write();
    delay(200);
  }
  
  addHighScore("PLAYER", snakeScore, "SNAKE");
  
  delay(3000);
  snakeGameActive = false;
  returnToMenu();
  
  triggerGameEvent(EVENT_HIGH_SCORE);
}

// ===== LED CONTROL =====
void setupLED() {
  pinMode(ledRedPin, OUTPUT);
  pinMode(ledGreenPin, OUTPUT);
  pinMode(ledBluePin, OUTPUT);
  updateLED();
}

void handleLEDCommand(String command) {
  if (command.startsWith("LED_CONTROL:")) {
    // Parse LED control command
    String data = command.substring(12);
    int r = data.substring(0, data.indexOf(',')).toInt();
    data = data.substring(data.indexOf(',') + 1);
    int g = data.substring(0, data.indexOf(',')).toInt();
    data = data.substring(data.indexOf(',') + 1);
    int b = data.substring(0, data.indexOf(',')).toInt();
    data = data.substring(data.indexOf(',') + 1);
    int brightness = data.substring(0, data.indexOf(',')).toInt();
    data = data.substring(data.indexOf(',') + 1);
    String mode = data.substring(0, data.indexOf(','));
    data = data.substring(data.indexOf(',') + 1);
    bool isOn = data == "true";

    // Update LED state
    ledState.r = r;
    ledState.g = g;
    ledState.b = b;
    ledState.brightness = brightness;
    ledState.mode = mode;
    ledState.isOn = isOn;

    updateLED();
    
    // Save state after update
    saveLEDState();
  }
}

void updateLED() {
  if (!ledState.isOn && currentEvent == EVENT_NONE) {
    analogWrite(ledRedPin, 0);
    analogWrite(ledGreenPin, 0);
    analogWrite(ledBluePin, 0);
    return;
  }

  // Handle game events
  if (currentEvent != EVENT_NONE) {
    if (millis() - eventStartTime > EVENT_DURATION) {
      currentEvent = EVENT_NONE;
    } else {
      switch (currentEvent) {
        case EVENT_GAME_START:
          // Flash white
          analogWrite(ledRedPin, 255);
          analogWrite(ledGreenPin, 255);
          analogWrite(ledBluePin, 255);
          break;
        case EVENT_GAME_OVER:
          // Flash red
          analogWrite(ledRedPin, 255);
          analogWrite(ledGreenPin, 0);
          analogWrite(ledBluePin, 0);
          break;
        case EVENT_LEVEL_UP:
          // Flash green
          analogWrite(ledRedPin, 0);
          analogWrite(ledGreenPin, 255);
          analogWrite(ledBluePin, 0);
          break;
        case EVENT_BOSS_BATTLE:
          // Flash purple
          analogWrite(ledRedPin, 255);
          analogWrite(ledGreenPin, 0);
          analogWrite(ledBluePin, 255);
          break;
        case EVENT_HIGH_SCORE:
          // Flash gold
          analogWrite(ledRedPin, 255);
          analogWrite(ledGreenPin, 215);
          analogWrite(ledBluePin, 0);
          break;
        case EVENT_POWER_UP:
          // Flash blue
          analogWrite(ledRedPin, 0);
          analogWrite(ledGreenPin, 0);
          analogWrite(ledBluePin, 255);
          break;
      }
      return;
    }
  }

  // Normal LED operation continues...
  // Calculate brightness-adjusted values
  float brightness = ledState.brightness / 100.0;
  int r = ledState.r * brightness;
  int g = ledState.g * brightness;
  int b = ledState.b * brightness;

  // Apply mode-specific effects
  if (ledState.mode == "blink") {
    static bool blinkState = false;
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 500) {
      blinkState = !blinkState;
      lastBlink = millis();
    }
    if (!blinkState) {
      r = g = b = 0;
    }
  }
  else if (ledState.mode == "pulse") {
    static unsigned long lastPulse = 0;
    static int pulseValue = 0;
    static bool increasing = true;
    
    if (millis() - lastPulse > 20) {
      lastPulse = millis();
      if (increasing) {
        pulseValue += 5;
        if (pulseValue >= 100) {
          pulseValue = 100;
          increasing = false;
        }
      } else {
        pulseValue -= 5;
        if (pulseValue <= 0) {
          pulseValue = 0;
          increasing = true;
        }
      }
    }
    
    float pulseBrightness = pulseValue / 100.0;
    r *= pulseBrightness;
    g *= pulseBrightness;
    b *= pulseBrightness;
  }
  else if (ledState.mode == "rainbow") {
    static unsigned long lastRainbow = 0;
    static int hue = 0;
    
    if (millis() - lastRainbow > 50) {
      lastRainbow = millis();
      hue = (hue + 1) % 360;
    }
    
    // Convert HSV to RGB
    float h = hue / 60.0;
    float s = 1.0;
    float v = brightness;
    
    float c = v * s;
    float x = c * (1 - abs(fmod(h, 2) - 1));
    float m = v - c;
    
    if (h < 1) { r = c; g = x; b = 0; }
    else if (h < 2) { r = x; g = c; b = 0; }
    else if (h < 3) { r = 0; g = c; b = x; }
    else if (h < 4) { r = 0; g = x; b = c; }
    else if (h < 5) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }
    
    r = (r + m) * 255;
    g = (g + m) * 255;
    b = (b + m) * 255;
  }

  // Write to LED pins
  analogWrite(ledRedPin, r);
  analogWrite(ledGreenPin, g);
  analogWrite(ledBluePin, b);
}

void saveLEDState() {
  EEPROM.put(EEPROM_LED_STATE_ADDR, ledState);
}

void loadLEDState() {
  EEPROM.get(EEPROM_LED_STATE_ADDR, ledState);
  // Validate loaded state
  if (ledState.r < 0 || ledState.r > 255 ||
      ledState.g < 0 || ledState.g > 255 ||
      ledState.b < 0 || ledState.b > 255 ||
      ledState.brightness < 0 || ledState.brightness > 100) {
    // Reset to default if invalid
    ledState = {0, 0, 0, 100, "solid", false};
  }
  updateLED();
}

void triggerGameEvent(GameEvent event) {
  currentEvent = event;
  eventStartTime = millis();
}
