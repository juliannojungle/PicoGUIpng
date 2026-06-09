#!/bin/bash
set -e

echo "=== Installing RP2040 toolchain ==="

sudo apt-get update
sudo apt-get install -y git cmake gcc-arm-none-eabi libnewlib-arm-none-eabi \
    build-essential libstdc++-arm-none-eabi-newlib

echo "--- Cloning Pico SDK ---"
if [ ! -d ~/pico-sdk ]; then
    git clone --depth 1 https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
    cd ~/pico-sdk && git submodule update --init
else
    echo "pico-sdk already exists at ~/pico-sdk"
fi

echo "--- Adding PICO_SDK_PATH to environment ---"
grep -q 'PICO_SDK_PATH' ~/.bashrc || echo 'export PICO_SDK_PATH=$HOME/pico-sdk' >> ~/.bashrc

echo "=== Done! Restart terminal or run: source ~/.bashrc ==="
