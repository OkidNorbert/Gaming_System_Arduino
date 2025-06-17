class GameDashboard {
  constructor() {
    this.isConnected = false
    this.refreshRate = 2000
    this.autoRefresh = true
    this.soundEnabled = true
    this.currentScoreFilter = "all"
    this.ws = null

    this.gameData = {
      flappyScore: 0,
      flappyHigh: 0,
      snakeScore: 0,
      snakeLength: 2,
      pongScore: 0,
      pongHigh: 0,
      currentGame: "menu",
      wifiSignal: 0,
      uptime: 0,
      freeMemory: 0,
    }

    this.highScores = []
    this.ledController = new LEDController()

    this.init()
  }

  init() {
    this.loadSettings()
    this.setupEventListeners()
    this.initializeWebSocket()
    this.checkConnection()
    this.updateConnectionStatus(false)

    console.log("Arduino Gaming Dashboard initialized")
  }

  setupEventListeners() {
    // Control buttons
    document.querySelectorAll(".control-btn").forEach((btn) => {
      const action = btn.dataset.action
      if (action) {
        btn.addEventListener("mousedown", () => this.sendControl(action))
        btn.addEventListener("touchstart", (e) => {
          e.preventDefault()
          this.sendControl(action)
        })
      }
    })

    // Keyboard controls
    document.addEventListener("keydown", (e) => this.handleKeyboard(e))

    // Window events
    window.addEventListener("beforeunload", () => this.saveSettings())
  }

  initializeWebSocket() {
    const protocol = window.location.protocol === "https:" ? "wss:" : "ws:"
    const wsUrl = `${protocol}//${window.location.hostname}:81/`

    this.ws = new WebSocket(wsUrl)

    this.ws.onopen = () => {
      console.log("WebSocket connected")
      this.updateConnectionStatus(true)
    }

    this.ws.onmessage = (event) => {
      const data = JSON.parse(event.data)
      this.handleWebSocketMessage(data)
    }

    this.ws.onclose = () => {
      console.log("WebSocket disconnected")
      this.updateConnectionStatus(false)
      setTimeout(() => this.initializeWebSocket(), 3000)
    }

    this.ws.onerror = (error) => {
      console.error("WebSocket error:", error)
    }
  }

  handleWebSocketMessage(data) {
    switch (data.type) {
      case "gameData":
        this.updateGameData(data.payload)
        break
      case "highScores":
        this.highScores = data.payload.scores
        this.updateHighScoresDisplay()
        break
      case "systemStatus":
        this.updateSystemStatus(data.payload)
        break
    }
  }

  async checkConnection() {
    try {
      const response = await fetch("/api/status", {
        method: "GET",
        timeout: 5000,
      })

      if (response.ok) {
        const data = await response.json()
        this.updateConnectionStatus(data.arduinoConnected)
        if (data.arduinoConnected) {
          this.playSound("connect")
        }
      } else {
        this.updateConnectionStatus(false)
      }
    } catch (error) {
      console.warn("Connection check failed:", error)
      this.updateConnectionStatus(false)
    }
  }

  updateConnectionStatus(connected) {
    this.isConnected = connected
    const statusDot = document.getElementById("connectionStatus")
    const statusText = document.getElementById("connectionText")

    if (connected) {
      statusDot.classList.add("connected")
      statusText.textContent = "Connected"
    } else {
      statusDot.classList.remove("connected")
      statusText.textContent = "Disconnected"
    }
  }

  async fetchGameData() {
    try {
      const response = await fetch("/api/gamedata")
      if (response.ok) {
        const data = await response.json()
        this.updateGameData(data)
      }
    } catch (error) {
      console.error("Failed to fetch game data:", error)
      this.updateConnectionStatus(false)
    }
  }

  updateGameData(data) {
    // Update game stats
    document.getElementById("flappyScore").textContent = data.flappyScore || 0
    document.getElementById("flappyHigh").textContent = data.flappyHigh || 0
    document.getElementById("snakeScore").textContent = data.snakeScore || 0
    document.getElementById("snakeLength").textContent = data.snakeLength || 2
    document.getElementById("pongScore").textContent = data.pongScore || 0
    document.getElementById("pongHigh").textContent = data.pongHigh || 0

    // Update system stats
    document.getElementById("wifiSignal").textContent = this.formatSignalStrength(data.wifiSignal)
    document.getElementById("uptime").textContent = this.formatUptime(data.uptime)
    document.getElementById("freeMemory").textContent = this.formatMemory(data.freeMemory)
    document.getElementById("arduinoStatus").textContent = data.arduinoConnected ? "Connected" : "Disconnected"

    // Update game status
    this.updateGameStatus(data)

    // Update LED based on game
    this.ledController.updateGameSync(data.currentGame, data)
  }

  updateGameStatus(data) {
    const flappyStatus = document.getElementById("flappyStatus")
    const snakeStatus = document.getElementById("snakeStatus")
    const pongStatus = document.getElementById("pongStatus")

    // Reset all statuses
    ;[flappyStatus, snakeStatus, pongStatus].forEach((status) => {
      status.textContent = "Idle"
      status.style.background = "rgba(255,255,255,0.2)"
    })

    // Set active game status
    switch (data.currentGame) {
      case "flappy":
        flappyStatus.textContent = "Playing"
        flappyStatus.style.background = "rgba(76, 175, 80, 0.8)"
        break
      case "snake":
        snakeStatus.textContent = "Playing"
        snakeStatus.style.background = "rgba(76, 175, 80, 0.8)"
        break
      case "pong":
        pongStatus.textContent = "Playing"
        pongStatus.style.background = "rgba(76, 175, 80, 0.8)"
        break
    }
  }

  async fetchHighScores() {
    try {
      const response = await fetch("/api/highscores")
      if (response.ok) {
        const data = await response.json()
        this.highScores = data.scores || []
        this.updateHighScoresDisplay()
      }
    } catch (error) {
      console.error("Failed to fetch high scores:", error)
    }
  }

  updateHighScoresDisplay() {
    const tableContainer = document.getElementById("highScoresTable")

    let filteredScores = this.highScores
    if (this.currentScoreFilter !== "all") {
      filteredScores = this.highScores.filter((score) => score.game.toLowerCase().includes(this.currentScoreFilter))
    }

    filteredScores.sort((a, b) => b.score - a.score)

    if (filteredScores.length === 0) {
      tableContainer.innerHTML = '<div class="loading">No high scores available</div>'
      return
    }

    let html = `
      <table>
        <thead>
          <tr>
            <th>Rank</th>
            <th>Player</th>
            <th>Game</th>
            <th>Score</th>
          </tr>
        </thead>
        <tbody>
    `

    filteredScores.slice(0, 10).forEach((score, index) => {
      const rankIcon = this.getRankIcon(index + 1)
      html += `
        <tr>
          <td>${rankIcon} ${index + 1}</td>
          <td>${score.player}</td>
          <td>${score.game}</td>
          <td>${score.score.toLocaleString()}</td>
        </tr>
      `
    })

    html += "</tbody></table>"
    tableContainer.innerHTML = html
  }

  getRankIcon(rank) {
    switch (rank) {
      case 1:
        return '<span class="rank-medal">🥇</span>'
      case 2:
        return '<span class="rank-medal">🥈</span>'
      case 3:
        return '<span class="rank-medal">🥉</span>'
      default:
        return ""
    }
  }

  async sendControl(action) {
    try {
      const response = await fetch("/api/control", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ action }),
      })

      if (response.ok) {
        this.playSound("button")
      }
    } catch (error) {
      console.error("Control command failed:", error)
    }
  }

  handleKeyboard(event) {
    const keyMap = {
      ArrowUp: "up",
      ArrowDown: "down",
      ArrowLeft: "left",
      ArrowRight: "right",
      Space: "fire",
      Enter: "fire",
      Escape: "menu",
    }

    const action = keyMap[event.code]
    if (action) {
      event.preventDefault()
      this.sendControl(action)
    }
  }

  // Utility Functions
  formatSignalStrength(rssi) {
    if (!rssi) return "Unknown"
    if (rssi > -50) return "Excellent"
    if (rssi > -60) return "Good"
    if (rssi > -70) return "Fair"
    return "Poor"
  }

  formatUptime(ms) {
    if (!ms) return "0s"
    const seconds = Math.floor(ms / 1000)
    const minutes = Math.floor(seconds / 60)
    const hours = Math.floor(minutes / 60)
    const days = Math.floor(hours / 24)

    if (days > 0) return `${days}d ${hours % 24}h`
    if (hours > 0) return `${hours}h ${minutes % 60}m`
    if (minutes > 0) return `${minutes}m ${seconds % 60}s`
    return `${seconds}s`
  }

  formatMemory(bytes) {
    if (!bytes) return "0 B"
    const kb = bytes / 1024
    if (kb < 1024) return `${kb.toFixed(1)} KB`
    const mb = kb / 1024
    return `${mb.toFixed(1)} MB`
  }

  playSound(type) {
    if (!this.soundEnabled) return

    try {
      const audioContext = new (window.AudioContext || window.webkitAudioContext)()
      const oscillator = audioContext.createOscillator()
      const gainNode = audioContext.createGain()

      oscillator.connect(gainNode)
      gainNode.connect(audioContext.destination)

      const frequencies = {
        button: 800,
        connect: 1000,
        error: 300,
      }

      oscillator.frequency.setValueAtTime(frequencies[type] || 600, audioContext.currentTime)
      oscillator.type = "square"

      gainNode.gain.setValueAtTime(0.1, audioContext.currentTime)
      gainNode.gain.exponentialRampToValueAtTime(0.01, audioContext.currentTime + 0.1)

      oscillator.start(audioContext.currentTime)
      oscillator.stop(audioContext.currentTime + 0.1)
    } catch (error) {
      console.warn("Audio not supported:", error)
    }
  }

  loadSettings() {
    const settings = localStorage.getItem("gameDashboardSettings")
    if (settings) {
      try {
        const parsed = JSON.parse(settings)
        this.refreshRate = parsed.refreshRate || 2000
        this.soundEnabled = parsed.soundEnabled !== false
        this.autoRefresh = parsed.autoRefresh !== false
      } catch (error) {
        console.warn("Failed to load settings:", error)
      }
    }
  }

  saveSettings() {
    const settings = {
      refreshRate: this.refreshRate,
      soundEnabled: this.soundEnabled,
      autoRefresh: this.autoRefresh,
    }
    localStorage.setItem("gameDashboardSettings", JSON.stringify(settings))
  }
}

// LED Controller Class
class LEDController {
  constructor() {
    this.color = "#ff0000"
    this.brightness = 255
    this.mode = "solid"
    this.isOn = false
    this.gameSync = false

    this.initializeEventListeners()
    this.updateInfo()
  }

  initializeEventListeners() {
    // Color picker
    document.getElementById("ledColor").addEventListener("input", (e) => {
      this.color = e.target.value
      this.updateLED()
    })

    // Brightness slider
    document.getElementById("ledBrightness").addEventListener("input", (e) => {
      this.brightness = e.target.value
      document.getElementById("brightnessValue").textContent = e.target.value
      this.updateLED()
    })

    // Mode buttons
    document.querySelectorAll(".mode-btn").forEach((btn) => {
      btn.addEventListener("click", () => {
        document.querySelectorAll(".mode-btn").forEach((b) => b.classList.remove("active"))
        btn.classList.add("active")
        this.mode = btn.dataset.mode
        this.gameSync = this.mode === "game"
        this.updateLED()
      })
    })

    // Power button
    document.getElementById("ledPower").addEventListener("click", () => {
      this.isOn = !this.isOn
      this.updateLED()
    })
  }

  updateLED() {
    this.updateInfo()
    this.sendToArduino()
  }

  updateGameSync(currentGame, gameData) {
    if (!this.gameSync || !this.isOn) return

    // Change LED color based on game
    const gameColors = {
      flappy: "#FFB86C",
      snake: "#4CAF50",
      pong: "#6C63FF",
      menu: "#FF6584",
    }

    const gameColor = gameColors[currentGame] || "#FFFFFF"
    if (gameColor !== this.color) {
      this.color = gameColor
      document.getElementById("ledColor").value = gameColor
      this.updateLED()
    }
  }

  updateInfo() {
    document.getElementById("currentColor").textContent = this.color
    document.getElementById("currentBrightness").textContent = `${Math.round((this.brightness / 255) * 100)}%`
    document.getElementById("currentMode").textContent = this.mode.charAt(0).toUpperCase() + this.mode.slice(1)
    document.getElementById("gameSyncStatus").textContent = this.gameSync ? "On" : "Off"
  }

  sendToArduino() {
    const rgb = this.hexToRgb(this.color)
    const command = {
      type: "LED_CONTROL",
      r: rgb.r,
      g: rgb.g,
      b: rgb.b,
      brightness: this.brightness,
      mode: this.mode,
      isOn: this.isOn,
      gameSync: this.gameSync,
    }

    // Send via WebSocket if available
    if (dashboard.ws && dashboard.ws.readyState === WebSocket.OPEN) {
      dashboard.ws.send(JSON.stringify(command))
    }
  }

  hexToRgb(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex)
    return result
      ? {
          r: Number.parseInt(result[1], 16),
          g: Number.parseInt(result[2], 16),
          b: Number.parseInt(result[3], 16),
        }
      : null
  }
}

// Global Functions
let dashboard

function refreshHighScores() {
  dashboard.fetchHighScores()
  dashboard.playSound("button")
}

// Initialize dashboard when page loads
document.addEventListener("DOMContentLoaded", () => {
  dashboard = new GameDashboard()
})
