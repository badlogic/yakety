import express from 'express'
import compression from 'compression'
import cors from 'cors'
import * as http from 'http'
import WebSocket, { WebSocketServer } from 'ws'
import * as chokidar from 'chokidar'
import * as fs from 'fs'

const port = process.env.PORT ?? 3333
const isDev = process.env.DEV === 'true'

async function main() {
  // Ensure data directory exists
  if (!fs.existsSync('/data')) {
    fs.mkdirSync('/data', { recursive: true })
  }
  if (!fs.existsSync('/data/logs')) {
    fs.mkdirSync('/data/logs', { recursive: true })
  }

  const app = express()
  app.set('json spaces', 2)
  app.use(cors())
  app.use(compression())
  app.use(express.json())
  app.use(express.urlencoded({ extended: true }))

  // API routes
  app.get('/api/hello', (req, res) => {
    res.json({
      message: 'Hello world from Yakety!',
      timestamp: new Date().toISOString(),
      environment: isDev ? 'development' : 'production'
    })
  })

  // Health check
  app.get('/api/health', (req, res) => {
    res.json({ status: 'ok', uptime: process.uptime() })
  })

  const server = http.createServer(app)

  // Set up live reload for development
  if (isDev) {
    setupLiveReload(server)
  }

  server.listen(port, () => {
    console.log(`🚀 Yakety website server listening on port ${port}`)
    console.log(`📡 Environment: ${isDev ? 'development' : 'production'}`)
  })
}

function setupLiveReload(server: http.Server) {
  const wss = new WebSocketServer({ server })
  const clients: Set<WebSocket> = new Set()

  wss.on('connection', (ws: WebSocket) => {
    console.log('🔌 Live reload client connected')
    clients.add(ws)
    ws.on('close', () => {
      clients.delete(ws)
    })
  })

  // Watch for changes in html directory (mounted as /www in container)
  chokidar.watch('/www/', { ignored: /(^|[\/\\])\../, ignoreInitial: true })
    .on('all', (event, filePath) => {
      console.log(`📁 File changed: ${filePath}`)
      clients.forEach((client) => {
        if (client.readyState === WebSocket.OPEN) {
          client.send(`reload:${filePath}`)
        }
      })
    })

  console.log('🔄 Live reload initialized')
}

main().catch(err => {
  console.error('❌ Server startup failed:', err)
  process.exit(1)
})