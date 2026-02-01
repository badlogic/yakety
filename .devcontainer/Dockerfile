FROM mcr.microsoft.com/devcontainers/cpp:ubuntu-24.04

# Enable universe and install build dependencies for Yakety
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive \
    && apt-get install -y --no-install-recommends software-properties-common \
    && add-apt-repository universe \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        ninja-build \
        pkg-config \
        libasound2-dev \
        libpulse-dev \
        libomp-dev \
        libevdev-dev \
        glslc \
        libvulkan-dev \
        vulkan-tools \
        curl \
        wget \
        xclip \
        xdotool \
        zenity \
    && apt-get autoremove -y && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/*
