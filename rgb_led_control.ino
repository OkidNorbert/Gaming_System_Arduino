// RGB LED Control
// Pins for RGB LED
const int redPin = 9;    // Red LED connected to pin 9
const int greenPin = 10; // Green LED connected to pin 10
const int bluePin = 11;  // Blue LED connected to pin 11

// Array of colors (R, G, B)
const int colors[][3] = {
  {255, 0, 0},    // Red
  {0, 255, 0},    // Green
  {0, 0, 255},    // Blue
  {255, 255, 0},  // Yellow
  {255, 0, 255},  // Magenta
  {0, 255, 255},  // Cyan
  {255, 255, 255} // White
};

const int numColors = sizeof(colors) / sizeof(colors[0]);
int currentColor = 0;

void setup() {
  // Initialize pins as outputs
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
}

void loop() {
  // Set the current color
  setColor(colors[currentColor][0], colors[currentColor][1], colors[currentColor][2]);
  
  // Move to next color
  currentColor = (currentColor + 1) % numColors;
  
  // Wait for 1 second before changing color
  delay(1000);
}

// Function to set RGB values
void setColor(int red, int green, int blue) {
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
} 