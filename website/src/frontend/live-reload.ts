if (
  window.location.hostname === 'localhost' ||
  window.location.hostname === '127.0.0.1'
) {
  const ws = new WebSocket(`ws://${window.location.hostname}:3333`)
  ws.onmessage = (event) => {
    if (event.data.startsWith('reload:')) {
      console.log('🔄 Reloading page due to file change')
      window.location.reload()
    }
  }
  ws.onopen = () => console.log('🔌 Live reload connected')
  ws.onclose = () => console.log('🔌 Live reload disconnected')
}
