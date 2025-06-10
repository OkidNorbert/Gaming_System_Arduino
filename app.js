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
    this.ws = null
    this.wsReconnectAttempts = 0
    this.maxReconnectAttempts = 5
    this.user = null
    this.isAuthenticated = false
    this.authToken = null

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
    this.userSettings = {}

    this.analytics = {
      sessionStats: {},
      gameStats: {
        ufo: {
          totalGames: 0,
          totalScore: 0,
          highScore: 0,
          averageScore: 0,
          totalPlayTime: 0,
          killsByLevel: {},
          deathsByLevel: {},
          accuracy: 0,
          powerupsCollected: 0
        },
        snake: {
          totalGames: 0,
          totalScore: 0,
          highScore: 0,
          averageScore: 0,
          totalPlayTime: 0,
          maxLength: 0,
          foodCollected: 0,
          averageLength: 0
        }
      },
      performance: {
        fps: [],
        latency: [],
        memoryUsage: []
      }
    }

    this.savedGames = {}
    this.autoSaveInterval = null
    this.autoSaveEnabled = true

    this.init()
  }

  init() {
    this.loadSettings()
    this.setupEventListeners()
    this.initializeCharts()
    this.initializeWebSocket()
    this.checkConnection()
    this.checkAuth()
    this.loadSavedGames()
    this.setupAutoSave()

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

  initializeWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    const wsUrl = `${protocol}//${window.location.host}/ws`
    
    this.ws = new WebSocket(wsUrl)
    
    this.ws.onopen = () => {
      console.log('WebSocket connected')
      this.wsReconnectAttempts = 0
      this.updateConnectionStatus(true)
    }
    
    this.ws.onmessage = (event) => {
      const data = JSON.parse(event.data)
      this.handleWebSocketMessage(data)
    }
    
    this.ws.onclose = () => {
      console.log('WebSocket disconnected')
      this.updateConnectionStatus(false)
      this.attemptReconnect()
    }
    
    this.ws.onerror = (error) => {
      console.error('WebSocket error:', error)
    }
  }

  attemptReconnect() {
    if (this.wsReconnectAttempts < this.maxReconnectAttempts) {
      this.wsReconnectAttempts++
      console.log(`Attempting to reconnect (${this.wsReconnectAttempts}/${this.maxReconnectAttempts})...`)
      setTimeout(() => this.initializeWebSocket(), 3000)
    } else {
      console.error('Max reconnection attempts reached')
      // Fallback to polling
      this.startDataRefresh()
    }
  }

  handleWebSocketMessage(data) {
    switch (data.type) {
      case 'gameData':
        this.updateGameData(data.payload)
        break
      case 'highScores':
        this.highScores = data.payload.scores
        this.updateHighScoresDisplay()
        break
      case 'systemStatus':
        this.updateSystemStatus(data.payload)
        break
      case 'gameState':
        this.updateGameState(data.payload)
        // Render game state if game view is enabled
        if (this.gameViewEnabled && this.gameRenderer[data.payload.game]) {
          this.gameRenderer[data.payload.game](data.payload.state)
        }
        break
    }
  }

  updateSystemStatus(data) {
    document.getElementById("wifiSignal").textContent = this.formatSignalStrength(data.wifiSignal)
    document.getElementById("uptime").textContent = this.formatUptime(data.uptime)
    document.getElementById("freeMemory").textContent = this.formatMemory(data.freeMemory)
    document.getElementById("arduinoStatus").textContent = data.arduinoConnected ? "Connected" : "Disconnected"
  }

  updateGameState(data) {
    const { game, state, score, level, lives } = data
    const gameElement = document.getElementById(`${game}Status`)
    if (gameElement) {
      gameElement.textContent = state
      gameElement.style.background = state === 'Playing' ? 'rgba(76, 175, 80, 0.8)' : 'rgba(255,255,255,0.2)'
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

    // Add user-specific indicators
    filteredScores = filteredScores.map(score => ({
      ...score,
      isCurrentUser: this.isAuthenticated && score.username === this.user.username
    }))

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

    // Initialize game canvas
    this.gameCanvas = document.getElementById('gameCanvas')
    this.gameCtx = this.gameCanvas.getContext('2d')
    this.setupGameCanvas()

    this.updateCharts()
  }

  setupGameCanvas() {
    // Set canvas size based on container
    const container = this.gameCanvas.parentElement
    this.gameCanvas.width = container.clientWidth
    this.gameCanvas.height = container.clientHeight

    // Initialize game renderer
    this.gameRenderer = {
      ufo: this.renderUFOGame.bind(this),
      snake: this.renderSnakeGame.bind(this),
      menu: this.renderMenu.bind(this)
    }
  }

  renderUFOGame(gameState) {
    const ctx = this.gameCtx
    const width = this.gameCanvas.width
    const height = this.gameCanvas.height

    // Clear canvas
    ctx.clearRect(0, 0, width, height)

    // Draw background
    ctx.fillStyle = '#000033'
    ctx.fillRect(0, 0, width, height)

    // Draw stars
    this.drawStars()

    // Draw player
    ctx.fillStyle = '#00ff00'
    ctx.fillRect(width/2 - 10, height - 30, 20, 20)

    // Draw UFOs
    gameState.ufos.forEach(ufo => {
      ctx.fillStyle = '#ff0000'
      ctx.beginPath()
      ctx.arc(ufo.x, ufo.y, 10, 0, Math.PI * 2)
      ctx.fill()
    })

    // Draw projectiles
    gameState.projectiles.forEach(proj => {
      ctx.fillStyle = '#ffff00'
      ctx.fillRect(proj.x - 2, proj.y - 8, 4, 8)
    })

    // Draw score and lives
    ctx.fillStyle = '#ffffff'
    ctx.font = '16px Arial'
    ctx.fillText(`Score: ${gameState.score}`, 10, 20)
    ctx.fillText(`Lives: ${gameState.lives}`, 10, 40)
  }

  renderSnakeGame(gameState) {
    const ctx = this.gameCtx
    const width = this.gameCanvas.width
    const height = this.gameCanvas.height
    const gridSize = 20

    // Clear canvas
    ctx.clearRect(0, 0, width, height)

    // Draw background
    ctx.fillStyle = '#000000'
    ctx.fillRect(0, 0, width, height)

    // Draw grid
    ctx.strokeStyle = '#333333'
    for(let x = 0; x < width; x += gridSize) {
      ctx.beginPath()
      ctx.moveTo(x, 0)
      ctx.lineTo(x, height)
      ctx.stroke()
    }
    for(let y = 0; y < height; y += gridSize) {
      ctx.beginPath()
      ctx.moveTo(0, y)
      ctx.lineTo(width, y)
      ctx.stroke()
    }

    // Draw snake
    gameState.snake.forEach((segment, index) => {
      ctx.fillStyle = index === 0 ? '#00ff00' : '#00cc00'
      ctx.fillRect(segment.x * gridSize, segment.y * gridSize, gridSize - 2, gridSize - 2)
    })

    // Draw food
    ctx.fillStyle = '#ff0000'
    ctx.fillRect(gameState.food.x * gridSize, gameState.food.y * gridSize, gridSize - 2, gridSize - 2)

    // Draw score
    ctx.fillStyle = '#ffffff'
    ctx.font = '16px Arial'
    ctx.fillText(`Score: ${gameState.score}`, 10, 20)
  }

  renderMenu() {
    const ctx = this.gameCtx
    const width = this.gameCanvas.width
    const height = this.gameCanvas.height

    // Clear canvas
    ctx.clearRect(0, 0, width, height)

    // Draw background
    ctx.fillStyle = '#000000'
    ctx.fillRect(0, 0, width, height)

    // Draw menu items
    ctx.fillStyle = '#ffffff'
    ctx.font = '24px Arial'
    ctx.textAlign = 'center'
    
    const menuItems = ['UFO Attack', 'Snake Game', 'Settings']
    menuItems.forEach((item, index) => {
      const y = height/2 - 50 + (index * 50)
      ctx.fillText(item, width/2, y)
    })
  }

  drawStars() {
    const ctx = this.gameCtx
    const width = this.gameCanvas.width
    const height = this.gameCanvas.height

    ctx.fillStyle = '#ffffff'
    for(let i = 0; i < 50; i++) {
      const x = Math.random() * width
      const y = Math.random() * height
      const size = Math.random() * 2
      ctx.fillRect(x, y, size, size)
    }
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

  async checkAuth() {
    const token = localStorage.getItem('authToken')
    if (token) {
      try {
        const response = await fetch('/api/auth/verify', {
          headers: {
            'Authorization': `Bearer ${token}`
          }
        })
        if (response.ok) {
          const data = await response.json()
          this.setUser(data.user)
        } else {
          this.logout()
        }
      } catch (error) {
        console.error('Auth verification failed:', error)
        this.logout()
      }
    }
  }

  setUser(userData) {
    this.user = userData
    this.isAuthenticated = true
    this.authToken = userData.token
    localStorage.setItem('authToken', userData.token)
    this.updateUserInterface()
    this.fetchUserSettings()
  }

  async login(username, password) {
    try {
      const response = await fetch('/api/auth/login', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({ username, password })
      })

      if (response.ok) {
        const data = await response.json()
        this.setUser(data)
        return true
      }
      return false
    } catch (error) {
      console.error('Login failed:', error)
      return false
    }
  }

  async register(username, password, email) {
    try {
      const response = await fetch('/api/auth/register', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({ username, password, email })
      })

      if (response.ok) {
        const data = await response.json()
        this.setUser(data)
        return true
      }
      return false
    } catch (error) {
      console.error('Registration failed:', error)
      return false
    }
  }

  logout() {
    this.user = null
    this.isAuthenticated = false
    this.authToken = null
    localStorage.removeItem('authToken')
    this.updateUserInterface()
  }

  updateUserInterface() {
    const authSection = document.getElementById('authSection')
    const userSection = document.getElementById('userSection')
    
    if (this.isAuthenticated) {
      authSection.style.display = 'none'
      userSection.style.display = 'block'
      document.getElementById('usernameDisplay').textContent = this.user.username
    } else {
      authSection.style.display = 'block'
      userSection.style.display = 'none'
    }
  }

  async fetchUserSettings() {
    if (!this.isAuthenticated) return

    try {
      const response = await fetch('/api/user/settings', {
        headers: {
          'Authorization': `Bearer ${this.authToken}`
        }
      })

      if (response.ok) {
        const data = await response.json()
        this.userSettings = data
        this.applyUserSettings()
      }
    } catch (error) {
      console.error('Failed to fetch user settings:', error)
    }
  }

  async saveUserSettings(settings) {
    if (!this.isAuthenticated) return

    try {
      const response = await fetch('/api/user/settings', {
        method: 'POST',
        headers: {
          'Authorization': `Bearer ${this.authToken}`,
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(settings)
      })

      if (response.ok) {
        this.userSettings = settings
        this.applyUserSettings()
      }
    } catch (error) {
      console.error('Failed to save user settings:', error)
    }
  }

  applyUserSettings() {
    // Apply user preferences
    if (this.userSettings.theme) {
      document.body.className = this.userSettings.theme
    }
    if (this.userSettings.refreshRate) {
      this.refreshRate = this.userSettings.refreshRate
      this.updateRefreshRateDisplay()
    }
    if (this.userSettings.soundEnabled !== undefined) {
      this.soundEnabled = this.userSettings.soundEnabled
    }
  }

  // Add analytics tracking methods
  trackGameEvent(game, event, data) {
    if (!this.analytics.gameStats[game]) return

    const stats = this.analytics.gameStats[game]
    
    switch (event) {
      case 'gameStart':
        stats.totalGames++
        this.startSession(game)
        break
      case 'gameEnd':
        this.endSession(game, data)
        break
      case 'score':
        stats.totalScore += data.score
        stats.highScore = Math.max(stats.highScore, data.score)
        stats.averageScore = stats.totalScore / stats.totalGames
        break
      case 'kill':
        if (game === 'ufo') {
          stats.killsByLevel[data.level] = (stats.killsByLevel[data.level] || 0) + 1
        }
        break
      case 'death':
        if (game === 'ufo') {
          stats.deathsByLevel[data.level] = (stats.deathsByLevel[data.level] || 0) + 1
        }
        break
      case 'food':
        if (game === 'snake') {
          stats.foodCollected++
          stats.maxLength = Math.max(stats.maxLength, data.length)
          stats.averageLength = (stats.averageLength * (stats.foodCollected - 1) + data.length) / stats.foodCollected
        }
        break
      case 'powerup':
        if (game === 'ufo') {
          stats.powerupsCollected++
        }
        break
    }

    this.updateAnalyticsDisplay()
  }

  startSession(game) {
    this.analytics.sessionStats[game] = {
      startTime: Date.now(),
      score: 0,
      events: []
    }
  }

  endSession(game, data) {
    const session = this.analytics.sessionStats[game]
    if (!session) return

    session.endTime = Date.now()
    session.duration = session.endTime - session.startTime
    session.finalScore = data.score

    const stats = this.analytics.gameStats[game]
    stats.totalPlayTime += session.duration

    // Calculate additional metrics
    if (game === 'ufo') {
      const totalShots = session.events.filter(e => e.type === 'shot').length
      const hits = session.events.filter(e => e.type === 'kill').length
      stats.accuracy = (hits / totalShots) * 100
    }

    this.saveAnalytics()
  }

  updateAnalyticsDisplay() {
    // Update UFO stats
    const ufoStats = this.analytics.gameStats.ufo
    document.getElementById('ufoTotalGames').textContent = ufoStats.totalGames
    document.getElementById('ufoHighScore').textContent = ufoStats.highScore
    document.getElementById('ufoAverageScore').textContent = Math.round(ufoStats.averageScore)
    document.getElementById('ufoAccuracy').textContent = `${Math.round(ufoStats.accuracy)}%`
    document.getElementById('ufoPowerups').textContent = ufoStats.powerupsCollected

    // Update Snake stats
    const snakeStats = this.analytics.gameStats.snake
    document.getElementById('snakeTotalGames').textContent = snakeStats.totalGames
    document.getElementById('snakeHighScore').textContent = snakeStats.highScore
    document.getElementById('snakeAverageScore').textContent = Math.round(snakeStats.averageScore)
    document.getElementById('snakeMaxLength').textContent = snakeStats.maxLength
    document.getElementById('snakeFoodCollected').textContent = snakeStats.foodCollected

    // Update performance metrics
    this.updatePerformanceMetrics()
  }

  updatePerformanceMetrics() {
    const fps = this.analytics.performance.fps
    const latency = this.analytics.performance.latency
    const memory = this.analytics.performance.memoryUsage

    document.getElementById('avgFPS').textContent = Math.round(this.calculateAverage(fps))
    document.getElementById('avgLatency').textContent = `${Math.round(this.calculateAverage(latency))}ms`
    document.getElementById('memoryUsage').textContent = this.formatMemory(this.calculateAverage(memory))
  }

  calculateAverage(array) {
    if (array.length === 0) return 0
    return array.reduce((a, b) => a + b, 0) / array.length
  }

  saveAnalytics() {
    if (this.isAuthenticated) {
      localStorage.setItem(`analytics_${this.user.username}`, JSON.stringify(this.analytics))
    }
  }

  loadAnalytics() {
    if (this.isAuthenticated) {
      const saved = localStorage.getItem(`analytics_${this.user.username}`)
      if (saved) {
        this.analytics = JSON.parse(saved)
        this.updateAnalyticsDisplay()
      }
    }
  }

  // Add analytics UI elements
  initializeAnalyticsUI() {
    const analyticsSection = document.createElement('div')
    analyticsSection.className = 'card analytics'
    analyticsSection.innerHTML = `
      <div class="card-header">
        <h2><i class="fas fa-chart-bar"></i> Detailed Analytics</h2>
      </div>
      <div class="card-content">
        <div class="analytics-grid">
          <div class="analytics-section">
            <h3>UFO Attack</h3>
            <div class="stat-grid">
              <div class="stat">
                <span class="stat-label">Total Games</span>
                <span class="stat-value" id="ufoTotalGames">0</span>
              </div>
              <div class="stat">
                <span class="stat-label">High Score</span>
                <span class="stat-value" id="ufoHighScore">0</span>
              </div>
              <div class="stat">
                <span class="stat-label">Average Score</span>
                <span class="stat-value" id="ufoAverageScore">0</span>
              </div>
              <div class="stat">
                <span class="stat-label">Accuracy</span>
                <span class="stat-value" id="ufoAccuracy">0%</span>
              </div>
              <div class="stat">
                <span class="stat-label">Powerups</span>
                <span class="stat-value" id="ufoPowerups">0</span>
              </div>
            </div>
          </div>
          <div class="analytics-section">
            <h3>Snake Game</h3>
            <div class="stat-grid">
              <div class="stat">
                <span class="stat-label">Total Games</span>
                <span class="stat-value" id="snakeTotalGames">0</span>
              </div>
              <div class="stat">
                <span class="stat-label">High Score</span>
                <span class="stat-value" id="snakeHighScore">0</span>
              </div>
              <div class="stat">
                <span class="stat-label">Average Score</span>
                <span class="stat-value" id="snakeAverageScore">0</span>
              </div>
              <div class="stat">
                <span class="stat-label">Max Length</span>
                <span class="stat-value" id="snakeMaxLength">0</span>
              </div>
              <div class="stat">
                <span class="stat-label">Food Collected</span>
                <span class="stat-value" id="snakeFoodCollected">0</span>
              </div>
            </div>
          </div>
          <div class="analytics-section">
            <h3>Performance</h3>
            <div class="stat-grid">
              <div class="stat">
                <span class="stat-label">Average FPS</span>
                <span class="stat-value" id="avgFPS">0</span>
              </div>
              <div class="stat">
                <span class="stat-label">Average Latency</span>
                <span class="stat-value" id="avgLatency">0ms</span>
              </div>
              <div class="stat">
                <span class="stat-label">Memory Usage</span>
                <span class="stat-value" id="memoryUsage">0MB</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    `

    document.querySelector('.dashboard').appendChild(analyticsSection)
  }

  setupAutoSave() {
    if (this.autoSaveInterval) {
      clearInterval(this.autoSaveInterval)
    }

    if (this.autoSaveEnabled) {
      this.autoSaveInterval = setInterval(() => {
        this.autoSave()
      }, 60000) // Auto-save every minute
    }
  }

  async saveGame(game, data) {
    if (!this.isAuthenticated) return false

    try {
      const saveData = {
        game,
        timestamp: Date.now(),
        data: {
          ...data,
          version: '1.0' // For future compatibility
        }
      }

      // Save locally
      this.savedGames[game] = saveData
      localStorage.setItem(`savedGame_${this.user.username}_${game}`, JSON.stringify(saveData))

      // Save to server
      const response = await fetch('/api/games/save', {
        method: 'POST',
        headers: {
          'Authorization': `Bearer ${this.authToken}`,
          'Content-Type': 'application/json'
        },
        body: JSON.stringify(saveData)
      })

      if (response.ok) {
        this.showNotification('Game saved successfully')
        return true
      }
      return false
    } catch (error) {
      console.error('Failed to save game:', error)
      return false
    }
  }

  async loadGame(game) {
    if (!this.isAuthenticated) return null

    try {
      // Try to load from server first
      const response = await fetch(`/api/games/load/${game}`, {
        headers: {
          'Authorization': `Bearer ${this.authToken}`
        }
      })

      if (response.ok) {
        const saveData = await response.json()
        this.savedGames[game] = saveData
        this.showNotification('Game loaded successfully')
        return saveData.data
      }

      // Fallback to local storage
      const localSave = localStorage.getItem(`savedGame_${this.user.username}_${game}`)
      if (localSave) {
        const saveData = JSON.parse(localSave)
        this.savedGames[game] = saveData
        this.showNotification('Game loaded from local storage')
        return saveData.data
      }

      return null
    } catch (error) {
      console.error('Failed to load game:', error)
      return null
    }
  }

  async deleteSave(game) {
    if (!this.isAuthenticated) return false

    try {
      // Delete from server
      const response = await fetch(`/api/games/delete/${game}`, {
        method: 'DELETE',
        headers: {
          'Authorization': `Bearer ${this.authToken}`
        }
      })

      // Delete locally
      delete this.savedGames[game]
      localStorage.removeItem(`savedGame_${this.user.username}_${game}`)

      if (response.ok) {
        this.showNotification('Save file deleted')
        return true
      }
      return false
    } catch (error) {
      console.error('Failed to delete save:', error)
      return false
    }
  }

  loadSavedGames() {
    if (!this.isAuthenticated) return

    // Load from local storage
    const games = ['ufo', 'snake']
    games.forEach(game => {
      const saved = localStorage.getItem(`savedGame_${this.user.username}_${game}`)
      if (saved) {
        this.savedGames[game] = JSON.parse(saved)
      }
    })

    this.updateSaveGameUI()
  }

  autoSave() {
    if (!this.isAuthenticated || !this.autoSaveEnabled) return

    const currentGame = this.gameData.currentGame
    if (currentGame && currentGame !== 'menu') {
      this.saveGame(currentGame, this.gameData)
    }
  }

  showNotification(message) {
    const notification = document.createElement('div')
    notification.className = 'notification'
    notification.textContent = message
    document.body.appendChild(notification)

    setTimeout(() => {
      notification.remove()
    }, 3000)
  }

  updateSaveGameUI() {
    const saveGameSection = document.getElementById('saveGameSection')
    if (!saveGameSection) return

    const currentGame = this.gameData.currentGame
    const hasSave = this.savedGames[currentGame]

    // Update save button
    const saveButton = document.getElementById('saveGameButton')
    if (saveButton) {
      saveButton.disabled = !this.isAuthenticated
      saveButton.textContent = hasSave ? 'Save Game (Overwrite)' : 'Save Game'
    }

    // Update load button
    const loadButton = document.getElementById('loadGameButton')
    if (loadButton) {
      loadButton.disabled = !this.isAuthenticated || !hasSave
    }

    // Update delete button
    const deleteButton = document.getElementById('deleteSaveButton')
    if (deleteButton) {
      deleteButton.disabled = !this.isAuthenticated || !hasSave
    }

    // Update last saved time
    const lastSaved = document.getElementById('lastSavedTime')
    if (lastSaved && hasSave) {
      const date = new Date(this.savedGames[currentGame].timestamp)
      lastSaved.textContent = `Last saved: ${date.toLocaleString()}`
    }
  }

  // Add save game UI elements
  initializeSaveGameUI() {
    const saveGameSection = document.createElement('div')
    saveGameSection.id = 'saveGameSection'
    saveGameSection.className = 'card save-game'
    saveGameSection.innerHTML = `
      <div class="card-header">
        <h2><i class="fas fa-save"></i> Save Game</h2>
        <div class="save-status">
          <span id="lastSavedTime">No save file</span>
        </div>
      </div>
      <div class="card-content">
        <div class="save-controls">
          <button id="saveGameButton" onclick="dashboard.saveGame(dashboard.gameData.currentGame, dashboard.gameData)">
            Save Game
          </button>
          <button id="loadGameButton" onclick="dashboard.loadGame(dashboard.gameData.currentGame)">
            Load Game
          </button>
          <button id="deleteSaveButton" onclick="dashboard.deleteSave(dashboard.gameData.currentGame)">
            Delete Save
          </button>
        </div>
        <div class="auto-save-controls">
          <label>
            <input type="checkbox" id="autoSaveCheckbox" 
              ${this.autoSaveEnabled ? 'checked' : ''} 
              onchange="dashboard.toggleAutoSave(this.checked)">
            Enable Auto-Save
          </label>
        </div>
      </div>
    `

    document.querySelector('.dashboard').appendChild(saveGameSection)
  }

  toggleAutoSave(enabled) {
    this.autoSaveEnabled = enabled
    this.setupAutoSave()
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

  // Add auth-related UI elements to the HTML
  const authSection = document.createElement('div')
  authSection.id = 'authSection'
  authSection.innerHTML = `
    <div class="auth-forms">
      <div class="login-form">
        <h3>Login</h3>
        <input type="text" id="loginUsername" placeholder="Username">
        <input type="password" id="loginPassword" placeholder="Password">
        <button onclick="dashboard.login(
          document.getElementById('loginUsername').value,
          document.getElementById('loginPassword').value
        )">Login</button>
      </div>
      <div class="register-form">
        <h3>Register</h3>
        <input type="text" id="registerUsername" placeholder="Username">
        <input type="email" id="registerEmail" placeholder="Email">
        <input type="password" id="registerPassword" placeholder="Password">
        <button onclick="dashboard.register(
          document.getElementById('registerUsername').value,
          document.getElementById('registerPassword').value,
          document.getElementById('registerEmail').value
        )">Register</button>
      </div>
    </div>
  `

  const userSection = document.createElement('div')
  userSection.id = 'userSection'
  userSection.innerHTML = `
    <div class="user-info">
      <span>Welcome, <span id="usernameDisplay"></span>!</span>
      <button onclick="dashboard.logout()">Logout</button>
    </div>
  `

  document.querySelector('.header-content').appendChild(authSection)
  document.querySelector('.header-content').appendChild(userSection)

  // Initialize analytics UI when the dashboard is created
  dashboard.initializeAnalyticsUI()
  dashboard.initializeSaveGameUI()
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

// LED Control
class LEDController {
  constructor() {
    this.colorPicker = document.querySelector('#ledColor');
    this.brightnessSlider = document.querySelector('#ledBrightness');
    this.modeButtons = document.querySelectorAll('.mode-btn');
    this.powerButton = document.querySelector('#ledPower');
    this.ledPreview = document.querySelector('.led-light');
    this.ledInfo = document.querySelector('.led-info');
    
    this.currentColor = '#ff0000';
    this.currentBrightness = 100;
    this.currentMode = 'solid';
    this.isOn = false;
    
    this.initializeEventListeners();
  }
  
  initializeEventListeners() {
    // Color picker
    this.colorPicker.addEventListener('input', (e) => {
      this.currentColor = e.target.value;
      this.updateLED();
    });
    
    // Brightness slider
    this.brightnessSlider.addEventListener('input', (e) => {
      this.currentBrightness = e.target.value;
      this.updateLED();
    });
    
    // Mode buttons
    this.modeButtons.forEach(btn => {
      btn.addEventListener('click', () => {
        this.modeButtons.forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        this.currentMode = btn.dataset.mode;
        this.updateLED();
      });
    });
    
    // Power button
    this.powerButton.addEventListener('click', () => {
      this.isOn = !this.isOn;
      this.powerButton.textContent = this.isOn ? 'Turn Off' : 'Turn On';
      this.powerButton.classList.toggle('active');
      this.updateLED();
    });
  }
  
  updateLED() {
    if (!this.isOn) {
      this.ledPreview.className = 'led-light off';
      this.ledPreview.style.backgroundColor = '#333';
      this.ledPreview.style.boxShadow = 'none';
      this.updateInfo();
      return;
    }
    
    // Update LED appearance
    this.ledPreview.className = 'led-light';
    this.ledPreview.style.backgroundColor = this.currentColor;
    
    // Calculate brightness-adjusted color
    const brightness = this.currentBrightness / 100;
    const rgb = this.hexToRgb(this.currentColor);
    const adjustedColor = `rgb(${rgb.r * brightness}, ${rgb.g * brightness}, ${rgb.b * brightness})`;
    
    // Apply mode-specific effects
    switch (this.currentMode) {
      case 'solid':
        this.ledPreview.style.boxShadow = `0 0 20px ${adjustedColor}`;
        break;
      case 'blink':
        this.ledPreview.classList.add('blink');
        break;
      case 'pulse':
        this.ledPreview.classList.add('pulse');
        break;
      case 'rainbow':
        this.ledPreview.classList.add('rainbow');
        break;
    }
    
    this.updateInfo();
    this.sendToArduino();
  }
  
  updateInfo() {
    const info = this.ledInfo;
    info.innerHTML = `
      <p>Color: <span>${this.currentColor}</span></p>
      <p>Brightness: <span>${this.currentBrightness}%</span></p>
      <p>Mode: <span>${this.currentMode}</span></p>
      <p>Status: <span>${this.isOn ? 'On' : 'Off'}</span></p>
    `;
  }
  
  hexToRgb(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
      r: parseInt(result[1], 16),
      g: parseInt(result[2], 16),
      b: parseInt(result[3], 16)
    } : null;
  }
  
  sendToArduino() {
    if (!this.isOn) {
      // Send command to turn off LED
      this.sendCommand('LED_OFF');
      return;
    }
    
    const rgb = this.hexToRgb(this.currentColor);
    const brightness = this.currentBrightness;
    const mode = this.currentMode;
    
    // Create command object
    const command = {
      type: 'LED_CONTROL',
      data: {
        r: rgb.r,
        g: rgb.g,
        b: rgb.b,
        brightness: brightness,
        mode: mode
      }
    };
    
    // Send command through WebSocket
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(command));
    }
  }
  
  sendCommand(command) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: command }));
    }
  }
}

// Initialize LED Controller when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
  const ledController = new LEDController();
});
