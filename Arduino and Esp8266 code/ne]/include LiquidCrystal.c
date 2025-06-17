#include <LiquidCrystal.h>
#include <EEPROM.h>

LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

const int joystickX = A0;
const int joystickY = A1;
const int buttonPin = 7;
const int buzzerPin = 8;

const int lcdCols = 16;
const int lcdRows = 2;

// ----- Game Selection -----
int selectedGame = 0;
const int totalGames = 3;

// ----- Scores -----
int highScoreFlappy = 0;
int highScoreSnake = 0;
int highScorePong = 0;

// ----- Custom Characters -----
byte birdChar[8] = { B00100, B01110, B11111, B10101, B11111, B01110, B00100, B00000 };
byte pipeChar[8] = { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111 };
byte snakeChar[8] = { B00100, B01110, B11111, B11111, B11111, B01110, B00100, B00000 };
byte ballChar[8] =  { B00000, B00110, B01111, B01111, B01111, B00110, B00000, B00000 };

// ----- Game Parameters -----
unsigned long lastFrame = 0;
const unsigned long frameDelay = 150;

// Flappy Bird variables
const int maxObstacles = 5;
int obstacleCols[maxObstacles];
int gapRows[maxObstacles];
int flappyRow = 0;
int flappyScore = 0;
int flappySpeed = 400;
bool flappyOver = false;

// Snake variables
const int snakeMaxLength = 16;
int snakeX[snakeMaxLength];
int snakeY[snakeMaxLength];
int snakeLength = 3;
int foodX, foodY;
int dirX = 1, dirY = 0;
int snakeScore = 0;
bool snakeOver = false;

// Pong variables
int ballX = 7, ballY = 0;
int ballVelX = 1, ballVelY = 1;
int paddleY = 0;
int pongScore = 0;
bool pongOver = false;

void setup() {
  lcd.begin(lcdCols, lcdRows);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(buzzerPin, OUTPUT);

  lcd.createChar(0, birdChar);
  lcd.createChar(1, pipeChar);
  lcd.createChar(2, snakeChar);
  lcd.createChar(3, ballChar);

  highScoreFlappy = EEPROM.read(0);
  highScoreSnake = EEPROM.read(1);
  highScorePong = EEPROM.read(2);

  showMenu();
}

void loop() {
  if (digitalRead(buttonPin) == LOW) {
    tone(buzzerPin, 1000, 50);
    delay(300);
    if (selectedGame == 0) playFlappyBird();
    if (selectedGame == 1) playSnake();
    if (selectedGame == 2) playPong();
    showMenu();
  }

  int joyVal = analogRead(joystickY);
  if (joyVal < 400) selectedGame = (selectedGame + 1) % totalGames;
  if (joyVal > 600) selectedGame = (selectedGame - 1 + totalGames) % totalGames;
  showMenu();
  delay(200);
}

void showMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select Game:");
  lcd.setCursor(0, 1);
  if (selectedGame == 0) lcd.print("> Flappy Bird");
  if (selectedGame == 1) lcd.print("> Snake");
  if (selectedGame == 2) lcd.print("> Pong");
}

//-------------------------------------------
// Game 1: Flappy Bird
//-------------------------------------------
void playFlappyBird() {
  flappyScore = 0; flappyRow = 0; flappySpeed = 400; flappyOver = false;
  for (int i = 0; i < maxObstacles; i++) {
    obstacleCols[i] = lcdCols + i * 4;
    gapRows[i] = random(0, 2);
  }
  while (!flappyOver) {
    if (millis() - lastFrame > flappySpeed) {
      lastFrame = millis();
      lcd.clear();
      lcd.setCursor(0, flappyRow);
      lcd.write(byte(0));
      for (int i = 0; i < maxObstacles; i++) {
        if (obstacleCols[i] >= 0 && obstacleCols[i] < lcdCols) {
          for (int r = 0; r < lcdRows; r++) {
            if (r != gapRows[i]) {
              lcd.setCursor(obstacleCols[i], r);
              lcd.write(byte(1));
            }
          }
        }
        if (obstacleCols[i] == 0 && flappyRow != gapRows[i]) {
          flappyOver = true;
          break;
        }
        obstacleCols[i]--;
        if (obstacleCols[i] < 0) {
          obstacleCols[i] = lcdCols + random(3, 8);
          gapRows[i] = random(0, 2);
          flappyScore++;
          if (flappySpeed > 150) flappySpeed -= 10;
        }
      }
      if (analogRead(joystickY) < 400) flappyRow = 0;
      if (analogRead(joystickY) > 600) flappyRow = 1;
    }
  }
  if (flappyScore > highScoreFlappy) {
    EEPROM.write(0, flappyScore);
    highScoreFlappy = flappyScore;
  }
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Game Over");
  lcd.setCursor(0,1); lcd.print("Score:"); lcd.print(flappyScore);
  delay(1500);
}

//-------------------------------------------
// Game 2: Snake
//-------------------------------------------
void playSnake() {
  snakeLength = 3; snakeX[0] = 5; snakeY[0] = 0; dirX = 1; dirY = 0; snakeScore = 0; snakeOver = false;
  placeFood();
  while (!snakeOver) {
    if (millis() - lastFrame > frameDelay) {
      lastFrame = millis();
      for (int i = snakeLength - 1; i > 0; i--) {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
      }
      snakeX[0] += dirX;
      snakeY[0] += dirY;
      if (snakeX[0] < 0 || snakeX[0] >= lcdCols || snakeY[0] < 0 || snakeY[0] >= lcdRows) snakeOver = true;
      for (int i = 1; i < snakeLength; i++) if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) snakeOver = true;
      if (snakeX[0] == foodX && snakeY[0] == foodY) { snakeLength++; placeFood(); snakeScore++; }
      lcd.clear();
      lcd.setCursor(foodX, foodY); lcd.write(byte(2));
      for (int i = 0; i < snakeLength; i++) {
        lcd.setCursor(snakeX[i], snakeY[i]);
        lcd.write(byte(2));
      }
      int joyX = analogRead(joystickX);
      int joyY = analogRead(joystickY);
      if (joyX < 400 && dirX != 1) { dirX = -1; dirY = 0; }
      if (joyX > 600 && dirX != -1) { dirX = 1; dirY = 0; }
      if (joyY < 400 && dirY != 1) { dirX = 0; dirY = -1; }
      if (joyY > 600 && dirY != -1) { dirX = 0; dirY = 1; }
    }
  }
  if (snakeScore > highScoreSnake) {
    EEPROM.write(1, snakeScore);
    highScoreSnake = snakeScore;
  }
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Game Over");
  lcd.setCursor(0,1); lcd.print("Score:"); lcd.print(snakeScore);
  delay(1500);
}

void placeFood() {
  foodX = random(0, lcdCols);
  foodY = random(0, lcdRows);
}

//-------------------------------------------
// Game 3: Pong
//-------------------------------------------
void playPong() {
  ballX = 7; ballY = 0; ballVelX = 1; ballVelY = 1; pongScore = 0; pongOver = false; paddleY = 0;
  while (!pongOver) {
    if (millis() - lastFrame > frameDelay) {
      lastFrame = millis();
      lcd.clear();
      lcd.setCursor(0, paddleY); lcd.print("|");
      lcd.setCursor(ballX, ballY); lcd.write(byte(3));
      ballX += ballVelX;
      ballY += ballVelY;
      if (ballY < 0 || ballY >= lcdRows) ballVelY *= -1;
      if (ballX == 1 && ballY == paddleY) { ballVelX *= -1; pongScore++; }
      if (ballX <= 0) pongOver = true;
      if (ballX >= lcdCols - 1) ballVelX *= -1;
      int joyY = analogRead(joystickY);
      if (joyY < 400) paddleY = 0;
      if (joyY > 600) paddleY = 1;
    }
  }
  if (pongScore > highScorePong) {
    EEPROM.write(2, pongScore);
    highScorePong = pongScore;
  }
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("Game Over");
  lcd.setCursor(0,1); lcd.print("Score:"); lcd.print(pongScore);
  delay(1500);
}

