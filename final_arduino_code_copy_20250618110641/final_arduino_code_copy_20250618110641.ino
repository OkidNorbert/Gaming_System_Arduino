#include <LiquidCrystal.h>
#include <EEPROM.h>
// #include <SoftwareSerial.h>

// Hardware Configuration
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);
const int joystickX = A0, joystickY = A1;
const int buttonPin = 7, buzzerPin = 8;
const int lcdCols = 16, lcdRows = 2;

// RGB LED pins (Common Anode - inverted logic)
const int redPin = 9;
const int greenPin = 10;
const int bluePin = 6;

// ESP8266 Communication
//SoftwareSerial esp8266(13, 14); // RX, TX
#define esp8266 Serial1  // Use Serial1 for ESP8266 (TX1=18, RX1=19)


// Game System
enum GameState { MENU, PLAYING, PAUSED, GAME_OVER };
enum GameType { FLAPPY_BIRD, SNAKE, PONG };

GameState currentState = MENU;
GameType selectedGame = FLAPPY_BIRD;
const int totalGames = 3;

// Enhanced Input System
struct InputState {
  bool buttonPressed = false;
  bool buttonHeld = false;
  bool lastButtonState = false;
  int joyX = 512, joyY = 512;
  bool joyLeft = false, joyRight = false, joyUp = false, joyDown = false;
  unsigned long lastButtonTime = 0;
  unsigned long lastJoyTime = 0;
  unsigned long lastMenuChange = 0;
  // Web control flags
  bool webJoyLeft = false, webJoyRight = false, webJoyUp = false, webJoyDown = false, webButtonPressed = false;
};

InputState input;

// RGB LED System with Default Settings
struct RGBLed {
  int r = 0, g = 100, b = 255;      // Default: Nice blue color
  int brightness = 200;              // Default: 80% brightness
  String mode = "pulse";             // Default: Pulse effect
  bool isOn = true;                  // Default: ON
  bool gameSync = false;             // Default: Manual control
  unsigned long lastUpdate = 0;
  int effectStep = 0;
} rgbLed;

// Graphics System
byte sprites[8][8] = {
  {B00100, B01110, B11111, B10101, B11111, B01110, B00100, B00000}, // Bird
  {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111}, // Wall
  {B01110, B11111, B11111, B11111, B11111, B11111, B01110, B00000}, // Snake
  {B00000, B00110, B01111, B01111, B01111, B00110, B00000, B00000}, // Ball
  {B11111, B10001, B10001, B10001, B10001, B10001, B10001, B11111}, // Brick
  {B00100, B01110, B01110, B11111, B11111, B01110, B01110, B00100}, // Power-up
  {B10101, B01010, B10101, B01010, B10101, B01010, B10101, B01010}, // Background
  {B11111, B00000, B11111, B00000, B11111, B00000, B11111, B00000}  // Paddle
};

// Score System
struct HighScores {
  byte flappy = 0;
  byte snake = 0;
  byte pong = 0;
  byte totalGamesPlayed = 0;
};

HighScores scores;

// Game Variables
int gameScore = 0;
bool gameOver = false;
unsigned long lastGameUpdate = 0;

// Flappy Bird Variables
int birdRow = 0;
int obstacles[3] = {16, 22, 28};
int gaps[3] = {0, 1, 0};

// Snake Variables
int snakeX[15], snakeY[15];
int snakeLength = 2;
int foodX = 8, foodY = 1;
int snakeDirX = 1, snakeDirY = 0;

// Pong Variables
int ballX = 8, ballY = 0;
int ballVelX = 1, ballVelY = 1;
int paddleY = 0;

// Communication Variables
unsigned long lastDataSend = 0;
const unsigned long dataSendInterval = 1000;

void setup() {
  Serial.begin(9600);
  esp8266.begin(9600);
  
  lcd.begin(lcdCols, lcdRows);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);
  
  // Setup RGB LED pins
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  
  // Initialize RGB LED with default settings
  Serial.println("Initializing RGB LED...");
  testRGBLED();  // Test all colors first
  
  // Load custom characters
  for(int i = 0; i < 8; i++) {
    lcd.createChar(i, sprites[i]);
  }
  
  loadScores();
  welcomeAnimation();
  currentState = MENU;
  showMenu();
  
  Serial.println("Arduino Gaming System v2.0 Started");
  Serial.println("RGB LED: Default blue pulse effect active");
  Serial.println("Menu: Use joystick UP/DOWN to select, button to confirm");
}

void loop() {
  updateInput();
  handleESP8266Communication();
  updateRGBLed();  // This will run the default pulse effect
  
  switch(currentState) {
    case MENU:
      handleMenu();
      break;
    case PLAYING:
      handleGameplay();
      break;
    case PAUSED:
      handlePause();
      break;
    case GAME_OVER:
      handleGameOver();
      break;
  }
  
  // Send data to ESP8266 periodically
  if (millis() - lastDataSend > dataSendInterval) {
    sendGameDataToESP();
    lastDataSend = millis();
  }
  // Remove the reset of web control flags here (now handled after use)
  delay(50);
}

// Test RGB LED function to verify wiring
void testRGBLED() {
  Serial.println("Testing RGB LED colors...");
  
  // Test Red
  Serial.println("Testing RED...");
  analogWrite(redPin, 0);    // ON for common anode
  analogWrite(greenPin, 255); // OFF
  analogWrite(bluePin, 255);  // OFF
  delay(1000);
  
  // Test Green
  Serial.println("Testing GREEN...");
  analogWrite(redPin, 255);   // OFF
  analogWrite(greenPin, 0);   // ON for common anode
  analogWrite(bluePin, 255);  // OFF
  delay(1000);
  
  // Test Blue
  Serial.println("Testing BLUE...");
  analogWrite(redPin, 255);   // OFF
  analogWrite(greenPin, 255); // OFF
  analogWrite(bluePin, 0);    // ON for common anode
  delay(1000);
  
  // Test White
  Serial.println("Testing WHITE...");
  analogWrite(redPin, 0);     // ON
  analogWrite(greenPin, 0);   // ON
  analogWrite(bluePin, 0);    // ON
  delay(1000);
  
  // Turn off
  Serial.println("RGB LED test complete");
  analogWrite(redPin, 255);   // OFF
  analogWrite(greenPin, 255); // OFF
  analogWrite(bluePin, 255);  // OFF
}

void updateInput() {
  unsigned long currentTime = millis();
  
  // Button handling with improved debouncing
  bool currentButtonState = digitalRead(buttonPin) == LOW;
  
  if(currentButtonState && !input.lastButtonState && 
     currentTime - input.lastButtonTime > 250) {
    input.buttonPressed = true;
    input.lastButtonTime = currentTime;
    playTone(1000, 100);
    Serial.println("Button pressed");
  } else {
    input.buttonPressed = false;
  }
  
  input.lastButtonState = currentButtonState;
  
  // Joystick handling with improved sensitivity
  if(currentTime - input.lastJoyTime > 150) {
    input.joyX = analogRead(joystickX);
    input.joyY = analogRead(joystickY);
    
    // Reset all directions
    input.joyLeft = false;
    input.joyRight = false;
    input.joyUp = false;
    input.joyDown = false;
    
    // Check joystick directions with better thresholds
    if(input.joyX < 300) {
      input.joyLeft = true;
      input.lastJoyTime = currentTime;
    }
    else if(input.joyX > 700) {
      input.joyRight = true;
      input.lastJoyTime = currentTime;
    }
    
    if(input.joyY < 300) {
      input.joyUp = true;
      input.lastJoyTime = currentTime;
    }
    else if(input.joyY > 700) {
      input.joyDown = true;
      input.lastJoyTime = currentTime;
    }
  }
  // Merge web controls with physical controls
  input.joyLeft  = input.joyLeft  || input.webJoyLeft;
  input.joyRight = input.joyRight || input.webJoyRight;
  input.joyUp    = input.joyUp    || input.webJoyUp;
  input.joyDown  = input.joyDown  || input.webJoyDown;
  input.buttonPressed = input.buttonPressed || input.webButtonPressed;
}

void handleESP8266Communication() {
  if (esp8266.available()) {
    String command = esp8266.readStringUntil('\n');
    command.trim();
    
    Serial.println("ESP8266 Command: " + command);
    
    if (command.startsWith("CONTROL:")) {
      String action = command.substring(8);
      handleRemoteControl(action);
    }
    else if (command.startsWith("LED:")) {
      handleLEDCommand(command.substring(4));
    }
    else if (command == "GET_DATA") {
      sendGameDataToESP();
    }
  }
}

void handleRemoteControl(String action) {
  Serial.println("Remote control: " + action);
  if (action == "up") input.webJoyUp = true;
  else if (action == "down") input.webJoyDown = true;
  else if (action == "left") input.webJoyLeft = true;
  else if (action == "right") input.webJoyRight = true;
  else if (action == "fire") input.webButtonPressed = true;
  else if (action == "menu") {
    currentState = MENU;
    showMenu();
  }
}

void handleLEDCommand(String ledData) {
  Serial.println("LED Command received: " + ledData);
  
  // Parse LED command: "r,g,b,brightness,mode,isOn,gameSync"
  int values[7];
  int valueIndex = 0;
  int lastIndex = 0;
  
  // Simple parsing
  for (int i = 0; i <= ledData.length() && valueIndex < 7; i++) {
    if (i == ledData.length() || ledData.charAt(i) == ',') {
      String valueStr = ledData.substring(lastIndex, i);
      
      if (valueIndex < 4) {
        values[valueIndex] = valueStr.toInt();
      } else if (valueIndex == 4) {
        // Mode string
        rgbLed.mode = valueStr;
      } else if (valueIndex == 5) {
        rgbLed.isOn = (valueStr == "1" || valueStr == "true");
      } else if (valueIndex == 6) {
        rgbLed.gameSync = (valueStr == "1" || valueStr == "true");
      }
      
      lastIndex = i + 1;
      valueIndex++;
    }
  }
  
  if (valueIndex >= 4) {
    rgbLed.r = values[0];
    rgbLed.g = values[1];
    rgbLed.b = values[2];
    rgbLed.brightness = values[3];
    
    Serial.println("LED Updated - R:" + String(rgbLed.r) + 
                   " G:" + String(rgbLed.g) + 
                   " B:" + String(rgbLed.b) + 
                   " Brightness:" + String(rgbLed.brightness) +
                   " Mode:" + rgbLed.mode +
                   " On:" + String(rgbLed.isOn));
  }
}

void updateRGBLed() {
  if (!rgbLed.isOn) {
    setRGBColor(0, 0, 0);
    return;
  }
  
  unsigned long currentTime = millis();
  
  if (rgbLed.gameSync) {
    updateGameSyncLED();
  } else if (rgbLed.mode == "solid") {
    setRGBColor(rgbLed.r, rgbLed.g, rgbLed.b);
  } else if (rgbLed.mode == "blink") {
    if (currentTime - rgbLed.lastUpdate > 500) {
      rgbLed.effectStep = !rgbLed.effectStep;
      rgbLed.lastUpdate = currentTime;
    }
    if (rgbLed.effectStep) {
      setRGBColor(rgbLed.r, rgbLed.g, rgbLed.b);
    } else {
      setRGBColor(0, 0, 0);
    }
  } else if (rgbLed.mode == "pulse") {
    if (currentTime - rgbLed.lastUpdate > 30) {
      rgbLed.effectStep = (rgbLed.effectStep + 3) % 360;
      rgbLed.lastUpdate = currentTime;
    }
    // Create smooth pulse effect
    float pulseValue = (sin(rgbLed.effectStep * 0.0174) + 1.0) / 2.0; // 0 to 1
    int pulseBrightness = (int)(pulseValue * rgbLed.brightness);
    
    setRGBColorWithBrightness(rgbLed.r, rgbLed.g, rgbLed.b, pulseBrightness);
  } else if (rgbLed.mode == "rainbow") {
    if (currentTime - rgbLed.lastUpdate > 50) {
      rgbLed.effectStep = (rgbLed.effectStep + 5) % 360;
      rgbLed.lastUpdate = currentTime;
    }
    setRainbowColor(rgbLed.effectStep);
  } else {
    // Default to pulse if unknown mode
    rgbLed.mode = "pulse";
  }
}

void updateGameSyncLED() {
  switch(selectedGame) {
    case FLAPPY_BIRD:
      if (currentState == PLAYING) {
        setRGBColor(255, 150, 0); // Orange
      } else {
        setRGBColor(255, 100, 150); // Pink
      }
      break;
    case SNAKE:
      if (currentState == PLAYING) {
        setRGBColor(0, 255, 0); // Green
      } else {
        setRGBColor(0, 200, 255); // Cyan
      }
      break;
    case PONG:
      if (currentState == PLAYING) {
        setRGBColor(100, 0, 255); // Purple
      } else {
        setRGBColor(255, 100, 150); // Pink
      }
      break;
  }
  
  // Flash on score
  static int lastScore = 0;
  if (gameScore > lastScore) {
    setRGBColor(255, 255, 255); // White flash
    delay(100);
    lastScore = gameScore;
  }
}

void setRGBColor(int red, int green, int blue) {
  setRGBColorWithBrightness(red, green, blue, rgbLed.brightness);
}

void setRGBColorWithBrightness(int red, int green, int blue, int brightness) {
  // Apply brightness
  red = (red * brightness) / 255;
  green = (green * brightness) / 255;
  blue = (blue * brightness) / 255;
  
  // For common anode RGB LED, invert the values (255 - value)
  analogWrite(redPin, 255 - red);
  analogWrite(greenPin, 255 - green);
  analogWrite(bluePin, 255 - blue);
}

void setRainbowColor(int hue) {
  int r, g, b;
  
  // Convert HSV to RGB
  if (hue < 60) {
    r = 255; g = (hue * 255) / 60; b = 0;
  } else if (hue < 120) {
    r = ((120 - hue) * 255) / 60; g = 255; b = 0;
  } else if (hue < 180) {
    r = 0; g = 255; b = ((hue - 120) * 255) / 60;
  } else if (hue < 240) {
    r = 0; g = ((240 - hue) * 255) / 60; b = 255;
  } else if (hue < 300) {
    r = ((hue - 240) * 255) / 60; g = 0; b = 255;
  } else {
    r = 255; g = 0; b = ((360 - hue) * 255) / 60;
  }
  
  setRGBColor(r, g, b);
}

void sendGameDataToESP() {
  String gameType = "";
  switch(selectedGame) {
    case FLAPPY_BIRD: gameType = "flappy"; break;
    case SNAKE: gameType = "snake"; break;
    case PONG: gameType = "pong"; break;
  }
  
  String stateType = "";
  switch(currentState) {
    case MENU: stateType = "menu"; break;
    case PLAYING: stateType = "playing"; break;
    case PAUSED: stateType = "paused"; break;
    case GAME_OVER: stateType = "gameover"; break;
  }
  
  String data = "DATA:";
  data += gameType + ",";
  data += stateType + ",";
  data += String(gameScore) + ",";
  data += String(scores.flappy) + ",";
  data += String(scores.snake) + ",";
  data += String(scores.pong) + ",";
  data += String(snakeLength) + ",";
  data += String(millis()) + ",";
  data += String(freeMemory()) + ",";
  data += String(rgbLed.r) + ",";
  data += String(rgbLed.g) + ",";
  data += String(rgbLed.b) + ",";
  data += String(rgbLed.brightness) + ",";
  data += rgbLed.mode + ",";
  data += String(rgbLed.isOn ? 1 : 0) + ",";
  data += String(rgbLed.gameSync ? 1 : 0);
  
  esp8266.println(data);
}

int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

void handleMenu() {
  unsigned long currentTime = millis();
  static GameType lastSelected = (GameType)-1;
  // Prevent rapid menu changes
  if(currentTime - input.lastMenuChange > 300) {
    if(input.joyUp) {
      selectedGame = (GameType)((selectedGame - 1 + totalGames) % totalGames);
      playTone(1500, 50);
      input.lastMenuChange = currentTime;
      Serial.print("Menu UP - Selected: ");
      Serial.println(selectedGame);
      input.webJoyUp = false; // Reset web flag after use
    }
    else if(input.joyDown) {
      selectedGame = (GameType)((selectedGame + 1) % totalGames);
      playTone(1500, 50);
      input.lastMenuChange = currentTime;
      Serial.print("Menu DOWN - Selected: ");
      Serial.println(selectedGame);
      input.webJoyDown = false; // Reset web flag after use
    }
  }
  // Update display only when selection changes
  if(selectedGame != lastSelected) {
    showMenu();
    lastSelected = selectedGame;
  }
  if(input.buttonPressed) {
    startGame();
    input.buttonPressed = false;
    input.webButtonPressed = false; // Reset web flag after use
  }
}

void handleGameplay() {
  switch(selectedGame) {
    case FLAPPY_BIRD:
      playFlappyBird();
      break;
    case SNAKE:
      playSnake();
      break;
    case PONG:
      playPong();
      break;
  }
  
  if(input.buttonPressed) {
    currentState = PAUSED;
    input.buttonPressed = false;
  }
}

void handlePause() {
  static bool pauseDisplayed = false;
  
  if(!pauseDisplayed) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PAUSED");
    lcd.setCursor(0, 1);
    lcd.print("Press to resume");
    pauseDisplayed = true;
  }
  
  if(input.buttonPressed) {
    currentState = PLAYING;
    pauseDisplayed = false;
    input.buttonPressed = false;
  }
}

void handleGameOver() {
  static unsigned long gameOverTime = 0;
  if(gameOverTime == 0) gameOverTime = millis();
  
  if(millis() - gameOverTime > 3000 || input.buttonPressed) {
    currentState = MENU;
    gameOverTime = 0;
    input.buttonPressed = false;
    showMenu();
  }
}

void startGame() {
  currentState = PLAYING;
  gameScore = 0;
  gameOver = false;
  lastGameUpdate = millis();
  
  Serial.print("Starting game: ");
  Serial.println(selectedGame);
  
  switch(selectedGame) {
    case FLAPPY_BIRD: 
      initFlappyBird(); 
      Serial.println("Flappy Bird initialized");
      break;
    case SNAKE: 
      initSnake(); 
      Serial.println("Snake initialized");
      break;
    case PONG:
      initPong();
      Serial.println("Pong initialized");
      break;
  }
  
  playTone(1000, 100);
}

void endGame(int finalScore) {
  currentState = GAME_OVER;
  scores.totalGamesPlayed++;
  
  switch(selectedGame) {
    case FLAPPY_BIRD:
      if(finalScore > scores.flappy) scores.flappy = finalScore;
      break;
    case SNAKE:
      if(finalScore > scores.snake) scores.snake = finalScore;
      break;
    case PONG:
      if(finalScore > scores.pong) scores.pong = finalScore;
      break;
  }
  
  saveScores();
  showGameOver(finalScore);
}

void showMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select Game:");
  lcd.setCursor(0, 1);
  
  switch(selectedGame) {
    case FLAPPY_BIRD: 
      lcd.print("> Flappy Bird"); 
      break;
    case SNAKE: 
      lcd.print("> Snake Game"); 
      break;
    case PONG: 
      lcd.print("> Pong Game"); 
      break;
  }
}

void showGameOver(int score) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Game Over!");
  lcd.setCursor(0, 1);
  lcd.print("Score: ");
  lcd.print(score);
}

void welcomeAnimation() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Arduino Gaming");
  lcd.setCursor(0, 1);
  lcd.print("System v2.0");
  
  // RGB welcome sequence
  rgbLed.isOn = true;
  for(int i = 0; i < 2; i++) {
    setRGBColor(255, 0, 0);    // Red
    delay(300);
    setRGBColor(0, 255, 0);    // Green
    delay(300);
    setRGBColor(0, 0, 255);    // Blue
    delay(300);
  }
  
  // Set to default blue pulse
  rgbLed.r = 0;
  rgbLed.g = 100;
  rgbLed.b = 255;
  rgbLed.mode = "pulse";
  rgbLed.isOn = true;
  
  delay(1000);
}

// ==================== FLAPPY BIRD ====================
void initFlappyBird() {
  birdRow = 0;
  obstacles[0] = 16;
  obstacles[1] = 22;
  obstacles[2] = 28;
  gaps[0] = random(0, 2);
  gaps[1] = random(0, 2);
  gaps[2] = random(0, 2);
}

void playFlappyBird() {
  unsigned long currentTime = millis();
  
  if(currentTime - lastGameUpdate > 300) {
    lastGameUpdate = currentTime;
    
    if(input.joyUp && birdRow > 0) birdRow = 0;
    if(input.joyDown && birdRow < 1) birdRow = 1;
    
    for(int i = 0; i < 3; i++) {
      obstacles[i]--;
      
      if(obstacles[i] == 1 && birdRow != gaps[i]) {
        gameOver = true;
        playTone(400, 500);
        break;
      }
      
      if(obstacles[i] < 0) {
        obstacles[i] = lcdCols + random(4, 8);
        gaps[i] = random(0, 2);
        gameScore++;
        playTone(800, 100);
      }
    }
  }
  
  lcd.clear();
  lcd.setCursor(1, birdRow);
  lcd.write(byte(0));
  
  for(int i = 0; i < 3; i++) {
    if(obstacles[i] >= 0 && obstacles[i] < lcdCols) {
      for(int row = 0; row < lcdRows; row++) {
        if(row != gaps[i]) {
          lcd.setCursor(obstacles[i], row);
          lcd.write(byte(1));
        }
      }
    }
  }
  
  lcd.setCursor(lcdCols - 2, 0);
  lcd.print(gameScore);
  
  if(gameOver) endGame(gameScore);
}

// ==================== SNAKE ====================
void initSnake() {
  snakeLength = 2;
  snakeX[0] = 4; snakeY[0] = 0;
  snakeX[1] = 3; snakeY[1] = 0;
  snakeDirX = 1; snakeDirY = 0;
  placeFood();
}

void placeFood() {
  do {
    foodX = random(0, lcdCols);
    foodY = random(0, lcdRows);
  } while(isSnakePosition(foodX, foodY));
}

bool isSnakePosition(int x, int y) {
  for(int i = 0; i < snakeLength; i++) {
    if(snakeX[i] == x && snakeY[i] == y) return true;
  }
  return false;
}

void playSnake() {
  unsigned long currentTime = millis();
  
  if(input.joyLeft && snakeDirX != 1) { snakeDirX = -1; snakeDirY = 0; input.webJoyLeft = false; }
  if(input.joyRight && snakeDirX != -1) { snakeDirX = 1; snakeDirY = 0; input.webJoyRight = false; }
  if(input.joyUp && snakeDirY != 1) { snakeDirX = 0; snakeDirY = -1; input.webJoyUp = false; }
  if(input.joyDown && snakeDirY != -1) { snakeDirX = 0; snakeDirY = 1; input.webJoyDown = false; }
  
  if(currentTime - lastGameUpdate > 500) {
    lastGameUpdate = currentTime;
    
    for(int i = snakeLength - 1; i > 0; i--) {
      snakeX[i] = snakeX[i-1];
      snakeY[i] = snakeY[i-1];
    }
    
    snakeX[0] += snakeDirX;
    snakeY[0] += snakeDirY;
    
    if(snakeX[0] < 0 || snakeX[0] >= lcdCols || snakeY[0] < 0 || snakeY[0] >= lcdRows) {
      gameOver = true;
      playTone(400, 500);
    }
    
    for(int i = 1; i < snakeLength; i++) {
      if(snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
        gameOver = true;
        playTone(400, 500);
      }
    }
    
    if(snakeX[0] == foodX && snakeY[0] == foodY) {
      if(snakeLength < 14) snakeLength++;
      gameScore++;
      placeFood();
      playTone(800, 100);
    }
  }
  
  lcd.clear();
  
  lcd.setCursor(foodX, foodY);
  lcd.print("o");
  
  for(int i = 0; i < snakeLength; i++) {
    if(snakeX[i] >= 0 && snakeX[i] < lcdCols && snakeY[i] >= 0 && snakeY[i] < lcdRows) {
      lcd.setCursor(snakeX[i], snakeY[i]);
      if(i == 0) lcd.print("#");
      else lcd.print("*");
    }
  }
  
  lcd.setCursor(lcdCols - 2, 1);
  lcd.print(gameScore);
  
  if(gameOver) endGame(gameScore);
}

// ==================== PONG ====================
void initPong() {
  ballX = 8; ballY = 0;
  ballVelX = 1; ballVelY = 1;
  paddleY = 0;
}

void playPong() {
  unsigned long currentTime = millis();
  
  if(currentTime - lastGameUpdate > 200) {
    lastGameUpdate = currentTime;
    
    if(input.joyUp && paddleY > 0) { paddleY = 0; input.webJoyUp = false; }
    if(input.joyDown && paddleY < 1) { paddleY = 1; input.webJoyDown = false; }
    
    ballX += ballVelX;
    ballY += ballVelY;
    
    if(ballY <= 0 || ballY >= lcdRows - 1) ballVelY *= -1;
    if(ballX >= lcdCols - 1) ballVelX *= -1;
    
    if(ballX == 1 && ballY == paddleY) {
      ballVelX = 1;
      gameScore++;
      playTone(800, 100);
    }
    
    if(ballX <= 0) {
      gameOver = true;
      playTone(400, 500);
    }
  }
  
  lcd.clear();
  
  lcd.setCursor(0, paddleY);
  lcd.print("|");
  
  lcd.setCursor(ballX, ballY);
  lcd.write(byte(3));
  
  lcd.setCursor(lcdCols - 2, 0);
  lcd.print(gameScore);
  
  if(gameOver) endGame(gameScore);
}

void playTone(int frequency, int duration) {
  tone(buzzerPin, frequency, duration);
}

void loadScores() {
  scores.flappy = EEPROM.read(0);
  scores.snake = EEPROM.read(1);
  scores.pong = EEPROM.read(2);
  scores.totalGamesPlayed = EEPROM.read(3);
}

void saveScores() {
  EEPROM.write(0, scores.flappy);
  EEPROM.write(1, scores.snake);
  EEPROM.write(2, scores.pong);
  EEPROM.write(3, scores.totalGamesPlayed);
}
