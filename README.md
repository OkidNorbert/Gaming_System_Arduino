# Arduino Gaming System with ESP8266 Web Dashboard

## Overview
This project is a modern, real-time gaming dashboard for Arduino, featuring:
- **Arduino Mega** for game logic and hardware control
- **ESP8266** as a WiFi web server, handling authentication, high scores, and real-time communication
- **Web dashboard** with a neon/dark theme, real-time controls, high score tracking, and user authentication

## Features
- Real-time game data and LED control via web interface
- Reliable serial communication between Arduino Mega and ESP8266
- WebSocket-based remote controls for games
- High score tracking with player differentiation
- Full authentication system (registration, login, session tokens)
- Guest mode for dashboard preview
- Modern, visually cohesive UI/UX

## Hardware Requirements
- Arduino Mega 2560
- ESP8266 (NodeMCU, Wemos D1 Mini, etc.)
- RGB LED (optional)
- Connection wires (TX/RX between Mega and ESP8266)

## Folder Structure
```
styles.css
final_arduino_code_copy_20250618110641/
  final_arduino_code_copy_20250618110641.ino

/gaming_web/
  /esp_code/
    esp_code.ino         # ESP8266 backend (web server, auth, high scores)
    /data/
      index.html         # Web dashboard UI
      app.js             # Dashboard logic, WebSocket, auth, high scores
      styles.css         # Dashboard and modal styles
```

## Setup Instructions

### 1. Arduino Mega
- Upload `final_arduino_code_copy_20250618110641.ino` to your Arduino Mega.
- Connect Mega TX1/RX1 (pins 18/19) to ESP8266 RX/TX (with voltage divider if needed).

### 2. ESP8266 (Web Server)
- Open `esp_code.ino` in Arduino IDE.
- Install required libraries: `ESP8266WiFi`, `ESPAsyncWebServer`, `ESPAsyncTCP`, `Hash`, `FS`/`LittleFS`, `ArduinoJson`, `WebSocketsServer`, `BearSSL`.
- Configure your WiFi credentials in the code.
- Upload `esp_code.ino` to your ESP8266.

#### Upload Web Files to SPIFFS/LittleFS
- In Arduino IDE, use the "ESP8266 Sketch Data Upload" tool (install via Tools > ESP8266 LittleFS Data Upload).
- Place `index.html`, `app.js`, and `styles.css` in the `/data` folder inside `esp_code`.
- Run the upload tool to copy web files to ESP8266 SPIFFS/LittleFS.

### 3. Access the Dashboard
- Connect to the ESP8266's IP address in your browser.
- Register a new account or use Guest mode to preview the dashboard.
- Use the web interface to control games, view high scores, and manage your account.

## Authentication & Guest Mode
- **Login/Register**: Modal overlay, not a separate page. Required for full access.
- **Guest Mode**: Allows dashboard preview and high score viewing, but restricts game controls and score submission.
- **Logout**: Button in the header when logged in.

## High Scores
- High scores are tracked per user and game.
- Guest users can view but not submit high scores.

## Customization
- Edit `styles.css` for UI theming.
- Add new games or features in `final_arduino_code_copy_20250618110641.ino` (Mega) and `esp_code.ino` (ESP8266).

## Troubleshooting
- If UI changes are not visible, clear your browser cache or use a private window.
- Ensure serial connections (TX/RX) are correct and baud rates match.
- For SPIFFS/LittleFS upload issues, check board selection and plugin installation.

## Credits
- UI icons: [Font Awesome](https://fontawesome.com/)
- Fonts: [Google Fonts - Orbitron](https://fonts.google.com/specimen/Orbitron)

## License
MIT License. See LICENSE file for details.
