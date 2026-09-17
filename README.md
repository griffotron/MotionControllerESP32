# Introduction 
This repository contains the code for ESP32 powered Motion Controller. See build video linked below.

It's tested in a Waveshare ESP32 S3 N16R8 Dev Board from Waveshare.

Build video to follow

# Getting Started
To compile an run you'll need to specify your own WiFi SSID and Password. There's a file named `wifi_creds.h.example`. Rename this to `wifi_creds.h` and add your own WiFi credentials so your ESP32 can be managed. Once connected, it should be accessible on your WiFi network via http://motioncontroller.local, though I've found using the IP address is more reliable and faster to load... but that may just be my WiFi.

# Building the UI
The UI is in the /ui folder and is built with Svelte + Vite. To run it for local development, open a terminal in VSCode, navigate to \ui (`cd ui`), then run `npm run dev`.

The PlatformIO build process runs `prebuild-ui.py` which takes the Svelte UI files and bundles them into C++ header files in the `include\ui` folder. They are then uploaded along with the rest of the C++ code.
