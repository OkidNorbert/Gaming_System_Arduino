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
#include <FastLED.h>

// ===== HARDWARE CONFIGURATION =====
#define NUM_LEDS 16
#define LED_PIN 6
#define JOYSTICK_X A0
#define JOYSTICK_Y A1
#define BUTTON_PIN 2
#define MIC_PIN A2

// LED control pins
const int ledPins[NUM_LEDS] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A3, A4, A5, 2, 3};

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
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t brightness;
  bool musicSync;
  bool isOn;
  uint8_t sensitivity;
  String mode;
} ledState = {0, 0, 0, 100, false, false, 50, "solid"};

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
  UFO_ATTACK_MODE,
  SNAKE_GAME,
  SETTINGS,
  HIGH_SCORES,
  PONG_GAME
};

GameState currentState = MAIN_MENU;
uint8_t selectedMenuItem = 0;
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
  {"Pong", "Classic tennis"},
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

// Microphone Module (4-pin: AO, G, +, DO)
const int micPin = A5; // AO pin of microphone module to Arduino A5
int micThreshold = 50; // Lower default threshold for better sensitivity
const int SAMPLE_WINDOW = 50; // Sample window for beat detection
int sampleBuffer[10]; // Buffer for smoothing
int sampleIndex = 0;
int sampleSum = 0;
unsigned long lastBeatTime = 0;
bool beatDetected = false;

// Add new LED modes
const char* ledModes[] = {"solid", "blink", "pulse", "rainbow", "music", "strobe", "wave", "colorcycle"};

// Move constant strings to PROGMEM
const char GAME_OVER[] PROGMEM = "GAME OVER";
const char HIGH_SCORE[] PROGMEM = "HIGH SCORE";
const char SCORE[] PROGMEM = "SCORE";
const char LEVEL[] PROGMEM = "LEVEL";
const char LIVES[] PROGMEM = "LIVES";
const char MENU[] PROGMEM = "MENU";
const char UFO_ATTACK[] PROGMEM = "UFO ATTACK";
const char SNAKE[] PROGMEM = "SNAKE";
const char LED_CONTROL[] PROGMEM = "LED_CONTROL";
const char MUSIC_SYNC[] PROGMEM = "MUSIC_SYNC";

// Reduce array sizes
#define MAX_SNAKE_LENGTH 32  // Reduced from 64
#define MAX_ENEMIES 8        // Reduced from 16
#define MAX_BULLETS 4        // Reduced from 8
#define SAMPLE_BUFFER_SIZE 16 // Reduced from 32

// Use smaller data types for coordinates
int8_t snakeX[MAX_SNAKE_LENGTH];
int8_t snakeY[MAX_SNAKE_LENGTH];
int8_t enemyX[MAX_ENEMIES];
int8_t enemyY[MAX_ENEMIES];
int8_t bulletX[MAX_BULLETS];
int8_t bulletY[MAX_BULLETS];
uint8_t sampleBuffer[SAMPLE_BUFFER_SIZE];  // Changed from uint16_t to uint8_t

// Optimize timing variables
uint16_t lastUpdate = 0;    // Changed from unsigned long
uint16_t lastSpawn = 0;     // Changed from unsigned long
uint16_t lastBeat = 0;      // Changed from unsigned long

// Optimize button handling
uint8_t lastButtonState = 0;
uint8_t currentButtonState = 0;
uint8_t lastDebounceTime = 0;

// Optimize menu
const uint8_t MENU_ITEMS = 2;
const char* const menuItems[] PROGMEM = {"SNAKE", "UFO ATTACK"};
uint8_t selectedMenuItem = 0;

// Optimize game variables
uint8_t snakeLength = 2;
uint8_t snakeDirection = 0;
uint8_t snakeSpeed = 1;
int8_t foodX = 0;
int8_t foodY = 0;

uint8_t playerX = 0;
uint8_t playerY = 0;
uint8_t enemyCount = 0;
uint8_t bulletCount = 0;

// Game modes
enum GameMode {
  SNAKE_MODE,
  UFO_MODE
};

// Game state structure
struct GameStateData {
  GameState mode;
  uint8_t score;
  uint8_t level;
  uint8_t lives;
  bool gameOver;
  uint8_t highScore;
} gameState = {MAIN_MENU, 0, 1, 5, false, 0};

// Snake game structure
struct SnakeGame {
  int8_t x[MAX_SNAKE_LENGTH];
  int8_t y[MAX_SNAKE_LENGTH];
  uint8_t length;
  uint8_t direction;
  uint8_t score;
  uint8_t speed;
  bool gameOver;
};

// UFO game structure
struct UFOGame {
  int8_t shipX;
  int8_t shipY;
  int8_t bulletX[MAX_BULLETS];
  int8_t bulletY[MAX_BULLETS];
  int8_t enemyX[MAX_ENEMIES];
  int8_t enemyY[MAX_ENEMIES];
  uint8_t score;
  uint8_t level;
  uint8_t lives;
  bool gameOver;
};

// Game state variables
GameStateData gameStateData = {MAIN_MENU, 0, 1, 5, false, 0};
SnakeGame snakeGame = {{0}, {0}, 2, 0, 0, 1, false};
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
#include <FastLED.h>

// ===== HARDWARE CONFIGURATION =====
#define NUM_LEDS 16
#define LED_PIN 6
#define JOYSTICK_X A0
#define JOYSTICK_Y A1
#define BUTTON_PIN 2
#define MIC_PIN A2

// LED control pins
const int ledPins[NUM_LEDS] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, A3, A4, A5, 2, 3};

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
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t brightness;
  bool musicSync;
  bool isOn;
  uint8_t sensitivity;
  String mode;
} ledState = {0, 0, 0, 100, false, false, 50, "solid"};

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
  UFO_ATTACK_MODE,
  SNAKE_GAME,
  SETTINGS,
  HIGH_SCORES,
  PONG_GAME
};

GameState currentState = MAIN_MENU;
uint8_t selectedMenuItem = 0;
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
  {"Pong", "Classic tennis"},
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

// Microphone Module (4-pin: AO, G, +, DO)
const int micPin = A5; // AO pin of microphone module to Arduino A5
int micThreshold = 50; // Lower default threshold for better sensitivity
const int SAMPLE_WINDOW = 50; // Sample window for beat detection
int sampleBuffer[10]; // Buffer for smoothing
int sampleIndex = 0;
int sampleSum = 0;
unsigned long lastBeatTime = 0;
bool beatDetected = false;

// Add new LED modes
const char* ledModes[] = {"solid", "blink", "pulse", "rainbow", "music", "strobe", "wave", "colorcycle"};

// Move constant strings to PROGMEM
const char GAME_OVER[] PROGMEM = "GAME OVER";
const char HIGH_SCORE[] PROGMEM = "HIGH SCORE";
const char SCORE[] PROGMEM = "SCORE";
const char LEVEL[] PROGMEM = "LEVEL";
const char LIVES[] PROGMEM = "LIVES";
const char MENU[] PROGMEM = "MENU";
const char UFO_ATTACK[] PROGMEM = "UFO ATTACK";
const char SNAKE[] PROGMEM = "SNAKE";
const char LED_CONTROL[] PROGMEM = "LED_CONTROL";
const char MUSIC_SYNC[] PROGMEM = "MUSIC_SYNC";

// Reduce array sizes
#define MAX_SNAKE_LENGTH 32  // Reduced from 64
#define MAX_ENEMIES 8        // Reduced from 16
#define MAX_BULLETS 4        // Reduced from 8
#define SAMPLE_BUFFER_SIZE 16 // Reduced from 32

// Use smaller data types for coordinates
int8_t snakeX[MAX_SNAKE_LENGTH];
int8_t snakeY[MAX_SNAKE_LENGTH];
int8_t enemyX[MAX_ENEMIES];
int8_t enemyY[MAX_ENEMIES];
int8_t bulletX[MAX_BULLETS];
int8_t bulletY[MAX_BULLETS];
uint8_t sampleBuffer[SAMPLE_BUFFER_SIZE];  // Changed from uint16_t to uint8_t

// Optimize timing variables
uint16_t lastUpdate = 0;    // Changed from unsigned long
uint16_t lastSpawn = 0;     // Changed from unsigned long
uint16_t lastBeat = 0;      // Changed from unsigned long

// Optimize button handling
uint8_t lastButtonState = 0;
uint8_t currentButtonState = 0;
uint8_t lastDebounceTime = 0;

// Optimize menu
const uint8_t MENU_ITEMS = 2;
const char* const menuItems[] PROGMEM = {"SNAKE", "UFO ATTACK"};
uint8_t selectedMenuItem = 0;

// Optimize game variables
uint8_t snakeLength = 2;
uint8_t snakeDirection = 0;
uint8_t snakeSpeed = 1;
int8_t foodX = 0;
int8_t foodY = 0;

uint8_t playerX = 0;
uint8_t playerY = 0;
uint8_t enemyCount = 0;
uint8_t bulletCount = 0;

// Game modes
enum GameMode {
  SNAKE_MODE,
  UFO_MODE
};

// Game state structure
struct GameStateData {
  GameState mode;
  uint8_t score;
  uint8_t level;
  uint8_t lives;
  bool gameOver;
  uint8_t highScore;
} gameState = {MAIN_MENU, 0, 1, 5, false, 0};

// Snake game structure
struct SnakeGame {
  int8_t x[MAX_SNAKE_LENGTH];
  int8_t y[MAX_SNAKE_LENGTH];
  uint8_t length;
  uint8_t direction;
  uint8_t speed;
  int8_t foodX;
  int8_t foodY;
  bool gameOver;
};

// UFO game structure
struct UFOGame {
  int8_t shipX;
  int8_t shipY;
  int8_t bulletX[MAX_BULLETS];
  int8_t bulletY[MAX_BULLETS];
  int8_t enemyX[MAX_ENEMIES];
  int8_t enemyY[MAX_ENEMIES];
  uint8_t score;
  uint8_t level;
  uint8_t lives;
  bool gameOver;
};

// Game state variables
GameStateData gameStateData = {MAIN_MENU, 0, 1, 5, false, 0};
SnakeGame snakeGame = {{0}, {0}, 2, 0, 1, 0, false};
UFOGame ufoGame = {0, 0, {0}, {0}, {0}, {0}, 0, 1, 5, false};
LEDState ledState = {255, 0, 0, 255, false, false, 50, "solid"};

// Menu items
const char* const menuItems[] PROGMEM = {"SNAKE", "UFO ATTACK"};
uint8_t selectedMenuItem = 0;

// Beat detection variables
uint8_t sampleBuffer[SAMPLE_BUFFER_SIZE];
uint8_t sampleIndex = 0;
uint8_t averageLevel = 0;
uint8_t threshold = 50;

// Button handling
uint8_t lastButtonState = 0;
uint8_t currentButtonState = 0;
uint8_t lastDebounceTime = 0;

// Timing variables
uint16_t lastUpdate = 0;
uint16_t lastSpawn = 0;
uint16_t lastBeat = 0;

// LED control functions
void setAllLEDs(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUM_LEDS; i++) {
    analogWrite(ledPins[i], (r + g + b) / 3);  // Simple brightness control
  }
}

void updateLED() {
  if (!ledState.isOn) {
    setAllLEDs(0, 0, 0);
    return;
  }

  // Convert mode string to integer for switch statement
  int modeIndex = 0;
  for (int i = 0; i < sizeof(ledModes) / sizeof(ledModes[0]); i++) {
    if (ledState.mode == ledModes[i]) {
      modeIndex = i;
      break;
    }
  }

  switch (modeIndex) {
    case 0: // solid
      setAllLEDs(ledState.r, ledState.g, ledState.b);
      break;
    case 1: // blink
      if (millis() % 1000 < 500) {
        setAllLEDs(ledState.r, ledState.g, ledState.b);
      } else {
        setAllLEDs(0, 0, 0);
      }
      break;
    case 2: // pulse
      {
        uint8_t brightness = (sin(millis() / 1000.0 * PI) + 1) * 127;
        setAllLEDs(
          ledState.r * brightness / 255,
          ledState.g * brightness / 255,
          ledState.b * brightness / 255
        );
      }
      break;
    case 3: // rainbow
      {
        uint8_t hue = millis() / 20;
        for (int i = 0; i < NUM_LEDS; i++) {
          setLEDColor(i, hue + i * 16, 255, ledState.brightness);
        }
      }
      break;
    case 4: // music
      if (ledState.musicSync) {
        updateMusicSync();
      }
      break;
    default:
      setAllLEDs(ledState.r, ledState.g, ledState.b);
      break;
  }
}

// ===== SETUP =====
void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  
  // Initialize display
  matrix.setIntensity(7);
  matrix.fillScreen(LOW);
  matrix.write();
  
  // Initialize LCD
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Gaming System");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");
  
  // Initialize LED pins
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(ledPins[i], OUTPUT);
  }
  
  // Initialize control pins
  pinMode(up, INPUT_PULLUP);
  pinMode(down, INPUT_PULLUP);
  pinMode(left, INPUT_PULLUP);
  pinMode(right, INPUT_PULLUP);
  pinMode(trigger, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  
  // Initialize game state
  currentState = MAIN_MENU;
  selectedMenuItem = 0;
  
  // Initialize snake game
  memset(snakeGame.x, 0, sizeof(snakeGame.x));
  memset(snakeGame.y, 0, sizeof(snakeGame.y));
  snakeGame.length = 2;
  snakeGame.direction = 0;
  snakeGame.score = 0;
  snakeGame.speed = 1;
  snakeGame.gameOver = false;
  
  // Initialize UFO game
  ufoGame.shipX = 0;
  ufoGame.shipY = 0;
  memset(ufoGame.bulletX, 0, sizeof(ufoGame.bulletX));
  memset(ufoGame.bulletY, 0, sizeof(ufoGame.bulletY));
  memset(ufoGame.enemyX, 0, sizeof(ufoGame.enemyX));
  memset(ufoGame.enemyY, 0, sizeof(ufoGame.enemyY));
  ufoGame.score = 0;
  ufoGame.level = 1;
  ufoGame.lives = 3;
  ufoGame.gameOver = false;
  
  // Initialize power state
  power.isSleeping = false;
  power.lastActivity = millis();
  
  // Initialize game flags
  pongGameActive = false;
  ufoGameActive = false;
  snakeGameActive = false;
  
  // Initialize LED state
  ledState.r = 0;
  ledState.g = 0;
  ledState.b = 0;
  ledState.brightness = 100;
  ledState.musicSync = false;
  ledState.isOn = false;
  ledState.sensitivity = 50;
  ledState.mode = "solid";
  
  // Initialize sample buffer
  memset(sampleBuffer, 0, sizeof(sampleBuffer));
  sampleIndex = 0;
  averageLevel = 0;
  threshold = 50;
  
  // Initialize button state
  lastButtonState = 0;
  currentButtonState = 0;
  lastDebounceTime = 0;
  
  // Initialize timing variables
  lastUpdate = 0;
  lastSpawn = 0;
  lastBeat = 0;
  
  // Load saved state
  loadLEDState();
  loadGameStats();
  
  // Show welcome message
  lcd.clear();
  lcd.print("Gaming System");
  lcd.setCursor(0, 1);
  lcd.print("Ready!");
  delay(1000);
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
    case UFO_ATTACK_MODE:
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
    case PONG_GAME:
      if (!pongGameActive) {
        startPong();
      }
      updatePong();
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
  
  // Update music sync
  updateMusicSync();
  
  // Update power management
  updatePowerManagement();
  
  // Skip main loop if sleeping
  if (power.isSleeping) {
    delay(100);
    return;
  }
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
    case UFO_ATTACK_MODE:
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
      currentState = UFO_ATTACK_MODE;
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
    case 2: // Pong
      currentState = PONG_GAME;
      break;
    case 3: // Back
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
    ledState = {0, 0, 0, 100, false, false, 50, "solid"};
  }
  updateLED();
}

void triggerGameEvent(GameEvent event) {
  currentEvent = event;
  eventStartTime = millis();
}

// Optimize detectBeat function
bool detectBeat() {
  if (!ledState.musicSync) return false;
  
  uint16_t currentLevel = analogRead(micPin);
  sampleBuffer[sampleIndex] = currentLevel;
  sampleIndex = (sampleIndex + 1) % SAMPLE_BUFFER_SIZE;
  
  // Calculate average
  uint32_t sum = 0;
  for (uint8_t i = 0; i < SAMPLE_BUFFER_SIZE; i++) {
    sum += sampleBuffer[i];
  }
  uint16_t averageLevel = sum / SAMPLE_BUFFER_SIZE;
  
  // Dynamic threshold
  uint16_t dynamicThreshold = averageLevel + (ledState.sensitivity * 2);
  
  // Beat detection
  if (currentLevel > dynamicThreshold && 
      (millis() - lastBeatTime) > 100) {  // Minimum 100ms between beats
    lastBeatTime = millis();
    return true;
  }
  
  return false;
}

// ===== PONG GAME VARIABLES =====
bool pongGameActive = false;
bool pongGameStart = false;

struct PongGame {
  int8_t paddle1Y;
  int8_t paddle2Y;
  int8_t ballX;
  int8_t ballY;
  int8_t ballDirX;
  int8_t ballDirY;
  int8_t score1;
  int8_t score2;
  uint8_t speed;
} pong;

void updatePong() {
  if (!pongGameActive) return;
  
  // Move paddles based on joystick input
  int joystickY = analogRead(JOYSTICK_Y);
  if (joystickY < 400) pong.paddle1Y = max(0, pong.paddle1Y - 1);
  if (joystickY > 600) pong.paddle1Y = min(7, pong.paddle1Y + 1);
  
  // AI for paddle2
  if (pong.ballY < pong.paddle2Y) pong.paddle2Y = max(0, pong.paddle2Y - 1);
  if (pong.ballY > pong.paddle2Y) pong.paddle2Y = min(7, pong.paddle2Y + 1);
  
  // Move ball
  pong.ballX += pong.ballDirX;
  pong.ballY += pong.ballDirY;
  
  // Ball collision with top and bottom
  if (pong.ballY <= 0 || pong.ballY >= 7) {
    pong.ballDirY *= -1;
    playTone(200, 50);
  }
  
  // Ball collision with paddles
  if (pong.ballX == 0 && pong.ballY >= pong.paddle1Y && pong.ballY <= pong.paddle1Y + 2) {
    pong.ballDirX *= -1;
    playTone(300, 50);
  }
  if (pong.ballX == 31 && pong.ballY >= pong.paddle2Y && pong.ballY <= pong.paddle2Y + 2) {
    pong.ballDirX *= -1;
    playTone(300, 50);
  }
  
  // Scoring
  if (pong.ballX < 0) {
    pong.score2++;
    resetPongBall();
    playTone(100, 200);
  }
  if (pong.ballX > 31) {
    pong.score1++;
    resetPongBall();
    playTone(100, 200);
  }
  
  // Update display
  matrix.fillScreen(LOW);
  
  // Draw paddles
  for (int i = 0; i < 3; i++) {
    matrix.drawPixel(0, pong.paddle1Y + i, HIGH);
    matrix.drawPixel(31, pong.paddle2Y + i, HIGH);
  }
  
  // Draw ball
  matrix.drawPixel(pong.ballX, pong.ballY, HIGH);
  
  // Draw score
  drawNumber(pong.score1, 8, 0);
  drawNumber(pong.score2, 24, 0);
  
  matrix.write();
  
  // Check for game over
  if (pong.score1 >= 5 || pong.score2 >= 5) {
    pongGameActive = false;
    currentState = GAME_MENU;
    showGameOver();
  }
}

void resetPongBall() {
  pong.ballX = 16;
  pong.ballY = random(1, 7);
  pong.ballDirX = (random(2) == 0) ? -1 : 1;
  pong.ballDirY = (random(2) == 0) ? -1 : 1;
}

void startPong() {
  pongGameActive = true;
  pong.paddle1Y = 3;
  pong.paddle2Y = 3;
  pong.score1 = 0;
  pong.score2 = 0;
  pong.speed = 1;
  resetPongBall();
  triggerGameEvent(EVENT_GAME_START);
}

// Enhanced Music Sync Variables
const int BEAT_THRESHOLD = 30;
const int BEAT_HOLD_TIME = 150;
const int BEAT_DECAY_RATE = 2;
const int MIN_BEAT_INTERVAL = 200;

struct MusicSync {
  int beatValue;
  int beatThreshold;
  unsigned long lastBeatTime;
  bool beatDetected;
  int energyLevel;
  int peakEnergy;
  uint8_t colorIndex;
} musicSync = {0, BEAT_THRESHOLD, 0, false, 0, 0, 0};

void updateMusicSync() {
  // Read microphone value
  int micValue = analogRead(MIC_PIN);
  
  // Calculate energy level
  musicSync.energyLevel = (musicSync.energyLevel * 7 + abs(micValue - 512)) / 8;
  
  // Update peak energy
  if (musicSync.energyLevel > musicSync.peakEnergy) {
    musicSync.peakEnergy = musicSync.energyLevel;
  } else {
    musicSync.peakEnergy = max(0, musicSync.peakEnergy - BEAT_DECAY_RATE);
  }
  
  // Beat detection
  unsigned long currentTime = millis();
  if (musicSync.energyLevel > musicSync.beatThreshold && 
      currentTime - musicSync.lastBeatTime > MIN_BEAT_INTERVAL) {
    musicSync.beatDetected = true;
    musicSync.lastBeatTime = currentTime;
    musicSync.beatValue = 255;
    musicSync.colorIndex = (musicSync.colorIndex + 1) % 255;
    
    // Trigger LED effect
    triggerGameEvent(EVENT_POWER_UP);
  }
  
  // Update LED colors based on music
  if (ledState.mode == "music") {
    // Calculate color based on energy level
    uint8_t hue = map(musicSync.energyLevel, 0, 1023, 0, 255);
    uint8_t brightness = map(musicSync.beatValue, 0, 255, 50, 255);
    
    // Apply color to LEDs
    for (int i = 0; i < NUM_LEDS; i++) {
      uint8_t ledHue = (hue + (i * 16)) % 255;
      setLEDColor(i, ledHue, 255, brightness);
    }
    
    // Decay beat value
    if (musicSync.beatValue > 0) {
      musicSync.beatValue = max(0, musicSync.beatValue - 5);
    }
  }
}

// ===== POWER MANAGEMENT =====
struct PowerState {
  bool isSleeping;
  unsigned long lastActivity;
} power = {false, 0};

void updatePowerManagement() {
  unsigned long currentTime = millis();
  
  // Check for user activity
  if (digitalRead(BUTTON_PIN) == LOW || 
      analogRead(JOYSTICK_X) < 400 || 
      analogRead(JOYSTICK_X) > 600 ||
      analogRead(JOYSTICK_Y) < 400 || 
      analogRead(JOYSTICK_Y) > 600) {
    power.lastActivity = currentTime;
    if (power.isSleeping) {
      wakeUp();
    }
  }
  
  // Check for sleep timeout
  if (!power.isSleeping && 
      currentTime - power.lastActivity > 300000) {
    enterSleep();
  }
  
  // Check battery level if analog pin is available
  #ifdef BATTERY_PIN
    int batteryLevel = analogRead(BATTERY_PIN);
    if (batteryLevel < 700) { // Low battery threshold
      // Reduce brightness
      matrix.setIntensity(0);
    } else {
      matrix.setIntensity(7);
    }
  #endif
}

void enterSleep() {
  // Save state before sleeping
  saveGameState();
  
  // Turn off all LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    setLEDColor(i, 0, 0, 0);
  }
  
  // Disable all peripherals
  matrix.setIntensity(0);
  lcd.noDisplay();
  
  // Enter low power mode
  power.isSleeping = true;
}

void wakeUp() {
  // Exit low power mode
  power.isSleeping = false;
  
  // Restore state after waking up
  loadLEDState();
  
  // Re-enable peripherals
  matrix.setIntensity(7);
  lcd.display();
}

void playTone(int frequency, int duration) {
  tone(buzzerPin, frequency, duration);
}

void drawNumber(int number, int x, int y) {
  char buffer[4];
  sprintf(buffer, "%d", number);
  matrix.drawChar(x, y, buffer[0], HIGH, LOW, 1);
  if (number >= 10) {
    matrix.drawChar(x + 6, y, buffer[1], HIGH, LOW, 1);
  }
}

void showGameOver() {
  matrix.fillScreen(LOW);
  matrix.drawChar(4, 0, 'G', HIGH, LOW, 1);
  matrix.drawChar(10, 0, 'O', HIGH, LOW, 1);
  matrix.write();
  delay(1000);
}

void setLEDColor(int index, uint8_t hue, uint8_t saturation, uint8_t brightness) {
  if (index >= 0 && index < NUM_LEDS) {
    analogWrite(ledPins[index], brightness);
  }
}

void saveGameState() {
  EEPROM.put(EEPROM_LED_STATE_ADDR, ledState);
  EEPROM.put(EEPROM_LED_STATE_ADDR + sizeof(LEDState), gameStats);
}

