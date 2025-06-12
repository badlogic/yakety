#!/bin/bash
set -e

echo "🔨 Building Yakety website..."
npm run build

echo "📦 Uploading to yakety.ai..."
rsync -avz --delete build/ slayer.marioslab.io:/home/badlogic/yakety.ai/build/
rsync -avz --delete html/ slayer.marioslab.io:/home/badlogic/yakety.ai/html/
rsync -avz --exclude='data' docker/ slayer.marioslab.io:/home/badlogic/yakety.ai/docker/

if [ "$1" == "server" ]; then
    echo "🔄 Restarting server..."
    ssh -t slayer.marioslab.io "cd /home/badlogic/yakety.ai && ./docker/control.sh stop && ./docker/control.sh start && ./docker/control.sh logs"
else
    echo "✅ Frontend-only deploy complete"
    echo "💡 Run './publish.sh server' to restart the backend"
fi

echo "🌐 Yakety website deployed to https://yakety.ai"