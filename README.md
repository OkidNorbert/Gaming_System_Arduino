# Arduino Gaming System - Web Files

## 📁 File Structure

\`\`\`
web-files/
├── index.html      # Main dashboard page
├── styles.css      # Complete CSS styling
├── app.js          # JavaScript dashboard logic
└── README.md       # This file
\`\`\`

## 🚀 Installation Options

### Option 1: Upload to ESP8266 LittleFS
1. Use ESP8266 Sketch Data Upload tool
2. Place files in `data/` folder in Arduino project
3. Upload to ESP8266 filesystem

### Option 2: Manual File Upload
1. Access ESP8266 web interface
2. Use `/upload` endpoint to upload files
3. Files will be stored in LittleFS

### Option 3: Embedded (Fallback)
- Files are embedded in ESP8266 code as fallback
- Automatically served if LittleFS files not found

## 🌐 Web Dashboard Features

### 📊 Real-time Monitoring
- Live game statistics (scores, lives, levels)
- System status (WiFi, memory, Arduino connection)
- High score tracking with automatic updates
- Game progress visualization

### 🎮 Remote Control
- Virtual D-pad for movement controls
- Fire and menu buttons
- Keyboard shortcuts (Arrow keys + Space/Enter)
- Mobile-friendly touch controls

### 🎵 Music Sync Lighting & Advanced LED Effects
- RGB LED can sync to music beats using a microphone module
- New LED modes: Music Sync, Strobe, Wave, Color Cycle, Rainbow, Pulse, Blink, Solid
- All LED modes controllable from the web dashboard
- Real-time color and brightness control
- Visual feedback for game events (e.g., game over, high score)

### 📈 Data Visualization
- Score progression charts
- Game session statistics
- Performance metrics
- Historical data tracking

### ⚙️ Settings & Customization
- Adjustable refresh rate
- Sound notifications toggle
- Dark mode support
- Auto-refresh controls

## 🔧 Hardware & Wiring

### Core Components
- Arduino Mega 2560 (**required for all connections and display support**)
- 3.2" ILI9341 TFT LCD Display with Resistive Touch (40-pin, parallel interface)
- TFT 3.2" Shield for Mega 2560 (required for safe connection)
- ESP8266 (NodeMCU/ESP-01, powered by its own micro USB)
- 16x2 or 20x4 LCD (I2C) *(optional, for non-TFT builds)*
- 8x8 LED Matrix (MAX7219, x2)
- Joystick Module (5-pin)
- Push Button(s)
- Buzzer (Passive or Active)
- RGB LED (Common Cathode)
- Microphone Module (4-pin: AO, G, +, DO)
- Potentiometer (optional)
- EEPROM (built-in)
- Breadboard, resistors, jumper wires

### Microphone Module Wiring (for Music Sync)
| Microphone Pin | Connects To      |
|---------------|------------------|
| AO (A0)       | Mega 2560 A5 (analog input) |
| DO            | (optional, not used for analog beat detection) |
| G             | Mega 2560 GND      |
| +             | Mega 2560 5V       |

### ESP8266 Power
- ESP8266 is powered by its own micro USB supply (not from Mega 2560 3.3V/5V)
- **GND of ESP8266 must be connected to Mega 2560 GND** for serial communication

### RGB LED Wiring
- R, G, B: Connect to Mega 2560 PWM pins (e.g., A2, A3, A4) via 220Ω resistors
- Cathode: GND

### 8x8 LED Matrix (MAX7219)
- DIN, CS, CLK: Connect to Mega 2560 digital pins (e.g., D10, D11, D12)
- VCC/GND: 5V and GND

### Joystick Module
- VRx, VRy: Mega 2560 analog pins (A0, A1)
- SW: Digital pin
- VCC/GND: 5V and GND

### LCD (I2C)
- SDA/SCL: Mega 2560 pins 20/21
- VCC/GND: 5V and GND

### Push Buttons, Buzzer, Potentiometer
- See COMPONENT_CONNECTIONS.md for full details (all connections to Mega 2560)

### ⚠️ TFT LCD Display Compatibility
- **Supported:** 3.2" ILI9341 TFT LCD (40-pin, parallel interface) **with Mega 2560 and TFT shield**
- **NOT Supported:** Arduino Uno, Nano, ESP32 (not enough pins, voltage incompatibility)
- **Shield Required:** Always use the TFT 3.2" shield with Mega 2560 to avoid damage
- **Do NOT use NEXTION libraries** (this is not a NEXTION display)
- **Works at 3.3V logic only** (shield handles voltage adaptation)
- **Do NOT connect directly to Uno/Nano/ESP32**

### 3.2" TFT LCD (ILI9341, 40-pin, parallel)
- Use with Mega 2560 and TFT 3.2" shield only
- Shield handles all wiring and voltage adaptation
- No manual wiring required if using shield
- For touch, use TouchScreen library (resistive analog)

## 🔄 API Endpoints

### Data Endpoints
- `GET /api/status` - System status
- `GET /api/gamedata` - Current game data
- `GET /api/highscores` - High score list
- `GET /api/stats` - Game statistics

### Control Endpoints
- `POST /api/control` - Send control commands
- `POST /api/led` - Send LED color/mode/brightness/music sync commands

### File Management
- `POST /upload` - Upload web files

## 🎨 Customization

### LED Modes
- Solid (default)
- Blink
- Pulse
- Rainbow
- Music Sync (reacts to music beats)
- Strobe
- Wave
- Color Cycle

### Themes
- Light mode (default)
- Dark mode toggle
- Custom CSS variables for easy theming

### Layout
- Responsive grid system
- Modular card-based design
- Easy to add/remove sections

## 📝 Troubleshooting

### Library Installation
- If you see `Adafruit_GFX.h: No such file or directory`, install the Adafruit GFX library via Library Manager or from [GitHub](https://github.com/adafruit/Adafruit-GFX-Library)
- For `Max72xxPanel.h`, install from [https://github.com/markruys/arduino-Max72xxPanel](https://github.com/markruys/arduino-Max72xxPanel) via ZIP
- Install `LiquidCrystal` and other libraries via Library Manager

### Upload Errors
- If you see `avrdude: stk500_recv(): programmer is not responding`:
  - Check Tools > Port and select the correct port
  - Check Tools > Board and select the correct board
  - For Nano, try both bootloader options
  - Try a different USB cable/port
  - Make sure no other program is using the port
  - Restart Arduino IDE

### Microphone Sensitivity
- Adjust `micThreshold` in the code for best beat detection
- Use AO (A0) for analog input; DO is optional

### ESP8266 Not Connecting
- Ensure ESP8266 is powered by micro USB
- Connect GND of ESP8266 to Mega 2560 GND
- Check serial wiring

## 📱 Mobile Support
- Responsive design for all screen sizes
- Touch-optimized controls
- Mobile-friendly interface
- Works on iOS and Android browsers

## 📋 Browser Compatibility
- Chrome/Chromium (recommended)
- Firefox
- Safari
- Edge
- Mobile browsers

## 📝 Notes
- **All connections and components are supported only with Arduino Mega 2560**
- **Do not attempt to use Uno/Nano/ESP32 for this project**
- For non-TFT builds, 16x2/20x4 LCD and LED matrix are supported on Mega 2560
- Files must be uploaded to ESP8266 LittleFS
- Total file size should be under 1MB
- Gzip compression recommended for large files
- Cache headers set for optimal performance

## 🆕 New Features (2024 Update)
- Music sync lighting with microphone module
- New LED modes: music, strobe, wave, colorcycle
- ESP8266 powered by micro USB (not Arduino)
- Improved troubleshooting and wiring instructions
- More robust and modern web dashboard
