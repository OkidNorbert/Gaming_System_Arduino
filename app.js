// Arduino Gaming System Web Dashboard
// Real-time monitoring and control interface

class GameDashboard {
  constructor() {
    this.isConnected = false
    this.refreshRate = 2000
    this.autoRefresh = true
    this.soundEnabled = true
    this.gameViewEnabled = false
    this.currentScoreFilter = "all"

    this.gameData = {
      ufoScore: 0,
      ufoLevel: 1,
      ufoLives: 5,
      ufoKills: 0,
      snakeScore: 0,
      snakeLength: 2,
      snakeSpeed: 1,
      currentGame: "menu",
      wifiSignal: 0,
      uptime: 0,
      freeMemory: 0,
    }

    this.highScores = []
    this.gameHistory = []

    this.init()
  }

  init() {
    this.loadSettings()
    this.setupEventListeners()
    this.initializeCharts()
    this.startDataRefresh()
    this.checkConnection()

    // Initialize UI
    this.updateRefreshRateDisplay()
    this.updateConnectionStatus(false)

    console.log("Arduino Gaming Dashboard initialized")
  }

  setupEventListeners() {
    // Keyboard controls
    document.addEventListener("keydown", (e) => this.handleKeyboard(e))
    document.addEventListener("keyup", (e) => this.handleKeyboardRelease(e))

    // Touch events for mobile
    this.setupTouchControls()

    // Window events
    window.addEventListener("beforeunload", () => this.saveSettings())
    window.addEventListener("resize", () => this.resizeCharts())

    // Settings events
    const refreshRateSlider = document.getElementById("refreshRate")
    if (refreshRateSlider) {
      refreshRateSlider.addEventListener("input", (e) => {
        this.refreshRate = Number.parseInt(e.target.value)
        this.updateRefreshRateDisplay()
      })
    }
  }

  setupTouchControls() {
    const controlButtons = document.querySelectorAll(".control-btn")
    controlButtons.forEach((btn) => {
      btn.addEventListener("touchstart", (e) => {
        e.preventDefault()
        btn.classList.add("active")
      })

      btn.addEventListener("touchend", (e) => {
        e.preventDefault()
        btn.classList.remove("active")
      })
    })
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

  startDataRefresh() {
    if (this.refreshInterval) {
      clearInterval(this.refreshInterval)
    }

    if (this.autoRefresh) {
      this.refreshInterval = setInterval(() => {
        this.fetchGameData()
        this.fetchHighScores()
      }, this.refreshRate)

      // Initial fetch
      this.fetchGameData()
      this.fetchHighScores()
    }
  }

  async fetchGameData() {
    try {
      const response = await fetch("/api/gamedata")
      if (response.ok) {
        const data = await response.json()
        this.updateGameData(data)
        this.updateLastUpdateTime()
      }
    } catch (error) {
      console.error("Failed to fetch game data:", error)
      this.updateConnectionStatus(false)
    }
  }

  updateGameData(data) {
    // Update UFO Attack stats
    document.getElementById("ufoScore").textContent = data.ufoScore || 0
    document.getElementById("ufoLevel").textContent = data.ufoLevel || 1
    document.getElementById("ufoLives").textContent = data.ufoLives || 5
    document.getElementById("ufoKills").textContent = data.ufoKills || 0

    // Update Snake stats
    document.getElementById("snakeScore").textContent = data.snakeScore || 0
    document.getElementById("snakeLength").textContent = data.snakeLength || 2
    document.getElementById("snakeSpeed").textContent = data.snakeSpeed || 1
    document.getElementById("snakeFood").textContent = data.snakeFood || 0

    // Update system stats
    document.getElementById("wifiSignal").textContent = this.formatSignalStrength(data.wifiSignal)
    document.getElementById("uptime").textContent = this.formatUptime(data.uptime)
    document.getElementById("freeMemory").textContent = this.formatMemory(data.freeMemory)
    document.getElementById("arduinoStatus").textContent = data.arduinoConnected ? "Connected" : "Disconnected"
    document.getElementById("currentGame").textContent = this.formatGameName(data.currentGame)

    // Update progress bars
    this.updateProgressBars(data)

    // Update game status
    this.updateGameStatus(data)

    // Store for history
    this.gameHistory.push({
      timestamp: Date.now(),
      ...data,
    })

    // Keep only last 100 entries
    if (this.gameHistory.length > 100) {
      this.gameHistory = this.gameHistory.slice(-100)
    }

    // Update charts
    this.updateCharts()
  }

  updateProgressBars(data) {
    // UFO progress (kills towards next level)
    const ufoProgress = document.getElementById("ufoProgress")
    if (ufoProgress) {
      const killsNeeded = (data.ufoLevel || 1) * 10
      const progressPercent = Math.min((((data.ufoKills || 0) % 10) / 10) * 100, 100)
      ufoProgress.style.width = progressPercent + "%"
    }

    // Snake progress (length towards max)
    const snakeProgress = document.getElementById("snakeProgress")
    if (snakeProgress) {
      const maxLength = 15
      const snakeProgressPercent = Math.min(((data.snakeLength || 2) / maxLength) * 100, 100)
      snakeProgress.style.width = snakeProgressPercent + "%"
    }
  }

  updateGameStatus(data) {
    const ufoStatus = document.getElementById("ufoStatus")
    const snakeStatus = document.getElementById("snakeStatus")

    // Determine game status based on current game and data
    if (data.currentGame === "ufo") {
      ufoStatus.textContent = "Playing"
      ufoStatus.style.background = "rgba(76, 175, 80, 0.8)"
      snakeStatus.textContent = "Idle"
      snakeStatus.style.background = "rgba(255,255,255,0.2)"
    } else if (data.currentGame === "snake") {
      snakeStatus.textContent = "Playing"
      snakeStatus.style.background = "rgba(76, 175, 80, 0.8)"
      ufoStatus.textContent = "Idle"
      ufoStatus.style.background = "rgba(255,255,255,0.2)"
    } else {
      ufoStatus.textContent = "Idle"
      ufoStatus.style.background = "rgba(255,255,255,0.2)"
      snakeStatus.textContent = "Idle"
      snakeStatus.style.background = "rgba(255,255,255,0.2)"
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

    // Sort by score descending
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
        return '<span class="rank-medal rank-1">🥇</span>'
      case 2:
        return '<span class="rank-medal rank-2">🥈</span>'
      case 3:
        return '<span class="rank-medal rank-3">🥉</span>'
      default:
        return ""
    }
  }

  initializeCharts() {
    // Initialize score chart
    const scoreCanvas = document.getElementById("scoreChart")
    if (scoreCanvas) {
      this.scoreChart = scoreCanvas.getContext("2d")
    }

    // Initialize session chart
    const sessionCanvas = document.getElementById("sessionChart")
    if (sessionCanvas) {
      this.sessionChart = sessionCanvas.getContext("2d")
    }

    this.updateCharts()
  }

  updateCharts() {
    this.drawScoreChart()
    this.drawSessionChart()
    this.updateStatsSummary()
  }

  drawScoreChart() {
    if (!this.scoreChart) return

    const canvas = document.getElementById("scoreChart")
    const ctx = this.scoreChart

    ctx.clearRect(0, 0, canvas.width, canvas.height)

    if (this.gameHistory.length < 2) return

    const data = this.gameHistory.slice(-20) // Last 20 data points
    const maxScore = Math.max(...data.map((d) => Math.max(d.ufoScore || 0, d.snakeScore || 0)))

    // Draw grid
    ctx.strokeStyle = "#e0e0e0"
    ctx.lineWidth = 1
    for (let i = 0; i <= 5; i++) {
      const y = (canvas.height / 5) * i
      ctx.beginPath()
      ctx.moveTo(0, y)
      ctx.lineTo(canvas.width, y)
      ctx.stroke()
    }

    // Draw UFO scores
    ctx.strokeStyle = "#ff6b6b"
    ctx.lineWidth = 2
    ctx.beginPath()
    data.forEach((point, index) => {
      const x = (canvas.width / (data.length - 1)) * index
      const y = canvas.height - ((point.ufoScore || 0) / maxScore) * canvas.height
      if (index === 0) ctx.moveTo(x, y)
      else ctx.lineTo(x, y)
    })
    ctx.stroke()

    // Draw Snake scores
    ctx.strokeStyle = "#4CAF50"
    ctx.lineWidth = 2
    ctx.beginPath()
    data.forEach((point, index) => {
      const x = (canvas.width / (data.length - 1)) * index
      const y = canvas.height - ((point.snakeScore || 0) / Math.max(100, maxScore)) * canvas.height
      if (index === 0) ctx.moveTo(x, y)
      else ctx.lineTo(x, y)
    })
    ctx.stroke()
  }

  drawSessionChart() {
    if (!this.sessionChart) return

    const canvas = document.getElementById("sessionChart")
    const ctx = this.sessionChart

    ctx.clearRect(0, 0, canvas.width, canvas.height)

    // Simple bar chart showing game distribution
    const games = ["UFO Attack", "Snake", "Menu"]
    const colors = ["#ff6b6b", "#4CAF50", "#2196F3"]
    const values = [40, 35, 25] // Demo values

    const barWidth = canvas.width / games.length
    const maxValue = Math.max(...values)

    games.forEach((game, index) => {
      const height = (values[index] / maxValue) * canvas.height * 0.8
      const x = index * barWidth + barWidth * 0.1
      const y = canvas.height - height

      ctx.fillStyle = colors[index]
      ctx.fillRect(x, y, barWidth * 0.8, height)

      // Label
      ctx.fillStyle = "#333"
      ctx.font = "12px Arial"
      ctx.textAlign = "center"
      ctx.fillText(values[index] + "%", x + barWidth * 0.4, y - 5)
    })
  }

  updateStatsSummary() {
    // Calculate summary statistics
    const totalGames = this.gameHistory.length
    const avgSession = totalGames > 0 ? "5m" : "0m" // Demo value
    const bestStreak = Math.max(...this.highScores.map((s) => s.score), 0)

    const totalGamesEl = document.getElementById("totalGames")
    const avgSessionEl = document.getElementById("avgSession")
    const bestStreakEl = document.getElementById("bestStreak")

    if (totalGamesEl) totalGamesEl.textContent = totalGames
    if (avgSessionEl) avgSessionEl.textContent = avgSession
    if (bestStreakEl) bestStreakEl.textContent = bestStreak.toLocaleString()
  }

  // Control Functions
  async sendControl(action) {
    try {
      const response = await fetch("/api/control", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ action }),
      })

      if (response.ok) {
        this.showControlFeedback(action)
        this.playSound("button")
      }
    } catch (error) {
      console.error("Control command failed:", error)
    }
  }

  showControlFeedback(action) {
    const statusElement = document.getElementById("controlsStatus")
    statusElement.textContent = `Sent: ${action.toUpperCase()}`
    statusElement.style.color = "#4CAF50"

    setTimeout(() => {
      statusElement.textContent = "Ready"
      statusElement.style.color = ""
    }, 1000)
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

  handleKeyboardRelease(event) {
    if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", "Space"].includes(event.code)) {
      this.sendControl("release")
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

  formatGameName(game) {
    const gameNames = {
      menu: "Main Menu",
      ufo: "UFO Attack",
      snake: "Snake Game",
    }
    return gameNames[game] || "Unknown"
  }

  updateLastUpdateTime() {
    const lastUpdateEl = document.getElementById("lastUpdate")
    if (lastUpdateEl) {
      lastUpdateEl.textContent = new Date().toLocaleTimeString()
    }
  }

  updateRefreshRateDisplay() {
    const refreshRateValueEl = document.getElementById("refreshRateValue")
    if (refreshRateValueEl) {
      refreshRateValueEl.textContent = this.refreshRate + "ms"
    }
  }

  playSound(type) {
    if (!this.soundEnabled) return

    // Create audio context for sound effects
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

  // Settings Functions
  loadSettings() {
    const settings = localStorage.getItem("gameDashboardSettings")
    if (settings) {
      try {
        const parsed = JSON.parse(settings)
        this.refreshRate = parsed.refreshRate || 2000
        this.soundEnabled = parsed.soundEnabled !== false
        this.autoRefresh = parsed.autoRefresh !== false

        // Apply settings to UI
        const refreshRateEl = document.getElementById("refreshRate")
        const soundEnabledEl = document.getElementById("soundEnabled")
        const autoRefreshEl = document.getElementById("autoRefresh")
        const darkModeEl = document.getElementById("darkMode")

        if (refreshRateEl) refreshRateEl.value = this.refreshRate
        if (soundEnabledEl) soundEnabledEl.checked = this.soundEnabled
        if (autoRefreshEl) autoRefreshEl.checked = this.autoRefresh

        if (parsed.darkMode && darkModeEl) {
          document.body.classList.add("dark-mode")
          darkModeEl.checked = true
        }
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
      darkMode: document.body.classList.contains("dark-mode"),
    }
    localStorage.setItem("gameDashboardSettings", JSON.stringify(settings))
  }

  resizeCharts() {
    // Redraw charts on window resize
    setTimeout(() => this.updateCharts(), 100)
  }
}

// Global Functions (called from HTML)
let dashboard

function updateRefreshRate() {
  const value = document.getElementById("refreshRate").value
  dashboard.refreshRate = Number.parseInt(value)
  dashboard.updateRefreshRateDisplay()
  dashboard.startDataRefresh()
}

function toggleSound() {
  dashboard.soundEnabled = document.getElementById("soundEnabled").checked
  if (dashboard.soundEnabled) {
    dashboard.playSound("button")
  }
}

function toggleDarkMode() {
  document.body.classList.toggle("dark-mode")
  dashboard.saveSettings()
}

function toggleAutoRefresh() {
  dashboard.autoRefresh = document.getElementById("autoRefresh").checked
  dashboard.startDataRefresh()
}

function saveSettings() {
  dashboard.saveSettings()
  dashboard.playSound("button")

  // Show confirmation
  const btn = event.target
  const originalText = btn.innerHTML
  btn.innerHTML = '<i class="fas fa-check"></i> Saved!'
  btn.style.background = "#4CAF50"

  setTimeout(() => {
    btn.innerHTML = originalText
    btn.style.background = ""
  }, 2000)
}

function resetSettings() {
  if (confirm("Reset all settings to default?")) {
    localStorage.removeItem("gameDashboardSettings")
    location.reload()
  }
}

function clearData() {
  if (confirm("Clear all stored data? This cannot be undone.")) {
    localStorage.clear()
    dashboard.gameHistory = []
    dashboard.highScores = []
    dashboard.updateHighScoresDisplay()
    dashboard.updateCharts()
    dashboard.playSound("button")
  }
}

function showScores(filter) {
  dashboard.currentScoreFilter = filter
  dashboard.updateHighScoresDisplay()

  // Update tab buttons
  document.querySelectorAll(".tab-btn").forEach((btn) => btn.classList.remove("active"))
  event.target.classList.add("active")
}

function refreshHighScores() {
  dashboard.fetchHighScores()
  dashboard.playSound("button")
}

function toggleGameView() {
  dashboard.gameViewEnabled = !dashboard.gameViewEnabled
  const icon = document.getElementById("viewIcon")

  if (dashboard.gameViewEnabled) {
    icon.className = "fas fa-eye-slash"
    // Start game visualization
    dashboard.startGameVisualization()
  } else {
    icon.className = "fas fa-eye"
    // Stop game visualization
    dashboard.stopGameVisualization()
  }
}

function sendControl(action) {
  dashboard.sendControl(action)
}

function updateStats() {
  const timeframe = document.getElementById("statsTimeframe").value
  console.log("Updating stats for timeframe:", timeframe)
  dashboard.updateCharts()
}

function showAbout() {
  document.getElementById("aboutModal").style.display = "block"
}

function showHelp() {
  document.getElementById("helpModal").style.display = "block"
}

function closeModal(modalId) {
  document.getElementById(modalId).style.display = "none"
}

// Initialize dashboard when page loads
document.addEventListener("DOMContentLoaded", () => {
  dashboard = new GameDashboard()

  // Close modals when clicking outside
  window.addEventListener("click", (event) => {
    if (event.target.classList.contains("modal")) {
      event.target.style.display = "none"
    }
  })
})

// Add game visualization methods to dashboard
GameDashboard.prototype.startGameVisualization = function () {
  const canvas = document.getElementById("gameCanvas")
  if (!canvas) return

  const ctx = canvas.getContext("2d")

  this.gameVisualizationInterval = setInterval(() => {
    this.drawGameState(ctx, canvas)
  }, 100)
}

GameDashboard.prototype.stopGameVisualization = function () {
  if (this.gameVisualizationInterval) {
    clearInterval(this.gameVisualizationInterval)
  }
}

GameDashboard.prototype.drawGameState = (ctx, canvas) => {
  // Clear canvas
  ctx.fillStyle = "#000"
  ctx.fillRect(0, 0, canvas.width, canvas.height)

  // Draw LED matrix simulation (32x8 grid)
  const ledWidth = canvas.width / 32
  const ledHeight = canvas.height / 8

  // Simulate some LEDs being on
  ctx.fillStyle = "#00ff00"
  for (let x = 0; x < 32; x++) {
    for (let y = 0; y < 8; y++) {
      if (Math.random() > 0.9) {
        // Random LEDs for demo
        ctx.fillRect(x * ledWidth, y * ledHeight, ledWidth - 1, ledHeight - 1)
      }
    }
  }

  // Draw grid lines
  ctx.strokeStyle = "#333"
  ctx.lineWidth = 0.5
  for (let x = 0; x <= 32; x++) {
    ctx.beginPath()
    ctx.moveTo(x * ledWidth, 0)
    ctx.lineTo(x * ledWidth, canvas.height)
    ctx.stroke()
  }
  for (let y = 0; y <= 8; y++) {
    ctx.beginPath()
    ctx.moveTo(0, y * ledHeight)
    ctx.lineTo(canvas.width, y * ledHeight)
    ctx.stroke()
  }
}
