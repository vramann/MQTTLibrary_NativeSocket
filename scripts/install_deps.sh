#!/usr/bin/env bash
# Install system packages required to build the native-socket MQTT library.
# All C++ library deps are pulled via CMake FetchContent automatically.
set -euo pipefail

if command -v apt-get &>/dev/null; then
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        libssl-dev \
        pkg-config
elif command -v brew &>/dev/null; then
    brew install cmake openssl@3 pkg-config
    export PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig"
else
    echo "Unsupported package manager. Install cmake, git, openssl-dev manually."
    exit 1
fi

echo "Dependencies installed."
