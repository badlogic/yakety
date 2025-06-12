#!/bin/bash
set -e

project="yakety"

printHelp() {
    echo "Usage: control.sh <command>"
    echo "Available commands:"
    echo
    echo "   start        Starts the services (Nginx, Node.js) in production mode"
    echo "   startdev     Starts the services in development mode with debugging"
    echo "   stop         Stops the services"
    echo "   logs         Shows live logs from all services"
    echo "   shell        Opens a shell into the Node.js container"
    echo "   shellnginx   Opens a shell into the Nginx container"
    echo "   restart      Restarts all services"
}

dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null && pwd)"
pushd "$dir" > /dev/null

case "$1" in
start)
    echo "🚀 Starting Yakety website in production mode..."
    docker compose -p $project -f docker-compose.base.yml -f docker-compose.prod.yml up -d
    ;;
startdev)
    echo "🛠️ Starting Yakety website in development mode..."
    docker compose -p $project -f docker-compose.base.yml -f docker-compose.dev.yml up --menu=false
    ;;
stop)
    echo "🛑 Stopping Yakety website..."
    docker compose -p $project -f docker-compose.base.yml down
    ;;
logs)
    echo "📋 Showing logs for Yakety website..."
    docker compose -p $project -f docker-compose.base.yml logs -f
    ;;
shell)
    echo "🐚 Opening shell in server container..."
    docker exec -it ${project}_server_1 sh
    ;;
shellnginx)
    echo "🐚 Opening shell in nginx container..."
    docker exec -it ${project}_web_1 sh
    ;;
restart)
    echo "🔄 Restarting Yakety website..."
    docker compose -p $project -f docker-compose.base.yml down
    docker compose -p $project -f docker-compose.base.yml -f docker-compose.prod.yml up -d
    ;;
*)
    echo "❌ Invalid command: $1"
    printHelp
    exit 1
    ;;
esac

popd > /dev/null