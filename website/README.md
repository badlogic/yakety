# Yakety Website

Website for Yakety - a voice-to-text application.

## Setup

```bash
npm install
npm run build
npm run dev
```

Available at:
- http://localhost:8080 (frontend)
- http://localhost:3333 (API)

## Deployment

```bash
./publish.sh
```

Builds and syncs to production server.

## Architecture

- **Frontend**: Lit + TailwindCSS, bundled with esbuild
- **Backend**: Express.js server
- **Docker**: nginx + Node.js