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

## 🔧 Configuration

### WiFi Settings
Update in `esp8266-web-server.ino`:
\`\`\`cpp
const char* ssid = "YourWiFiName";
const char* password = "YourWiFiPassword";
\`\`\`

### Access Methods
- **IP Address**: Shown on Arduino LCD during startup
- **Hostname**: `http://arduino-gaming.local`
- **Direct**: Check router for ESP8266 IP

## 📱 Mobile Support
- Responsive design for all screen sizes
- Touch-optimized controls
- Mobile-friendly interface
- Works on iOS and Android browsers

## 🔄 API Endpoints

### Data Endpoints
- `GET /api/status` - System status
- `GET /api/gamedata` - Current game data
- `GET /api/highscores` - High score list
- `GET /api/stats` - Game statistics

### Control Endpoints
- `POST /api/control` - Send control commands

### File Management
- `POST /upload` - Upload web files

## 🎨 Customization

### Themes
- Light mode (default)
- Dark mode toggle
- Custom CSS variables for easy theming

### Layout
- Responsive grid system
- Modular card-based design
- Easy to add/remove sections

## 🔧 Development

### Local Testing
1. Serve files with local web server
2. Update API endpoints to ESP8266 IP
3. Test responsive design

### File Modifications
- Edit HTML/CSS/JS files directly
- Upload to ESP8266 via web interface
- Changes take effect immediately

## 📋 Browser Compatibility
- Chrome/Chromium (recommended)
- Firefox
- Safari
- Edge
- Mobile browsers

## 🚨 Troubleshooting

### Files Not Loading
1. Check LittleFS upload
2. Verify file names match exactly
3. Check ESP8266 serial output for errors

### Controls Not Working
1. Verify Arduino-ESP8266 serial connection
2. Check WiFi connection status
3. Monitor browser console for errors

### Performance Issues
1. Adjust refresh rate in settings
2. Close other browser tabs
3. Check WiFi signal strength

## 📝 Notes
- Files must be uploaded to ESP8266 LittleFS
- Total file size should be under 1MB
- Gzip compression recommended for large files
- Cache headers set for optimal performance
