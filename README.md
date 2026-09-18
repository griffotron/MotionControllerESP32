# Introduction 
This repository contains the code for the ESP32 powered Motion Controller. See build video linked below.

It's tested in a Waveshare ESP32 S3 N16R8 Dev Board from Waveshare. It works the ESP32 hard and needs a good amount of memory, so you may not get it to run properly on lesser hardware (a C6 for example).

Please see the related repos for the [STM32 Motion Module](https://github.com/griffotron/MotionModuleSTM32R8) and [CAD Designs](https://github.com/griffotron/MechanicalDisplayCAD).

# Build Video
Click below to watch the full build video on YouTube
[![Watch the video](https://img.youtube.com/vi/k-LaMbClWiI/maxresdefault.jpg)](https://www.youtube.com/watch?v=k-LaMbClWiI)

# Getting Started
To compile the code you'll need to specify your own WiFi SSID and Password so you can access the UI in your browser. There's a file named `wifi_creds.h.example`. Rename this to `wifi_creds.h` and add your own WiFi credentials. Once connected, it should be accessible on your WiFi network via http://motioncontroller.local, though I've found using the IP address is more reliable and faster to load... but that may just be my WiFi.

# Building the UI
The UI is in the \ui folder and is built with Svelte + Vite. To run it for local development, open a terminal in VSCode, navigate to \ui (`cd ui`), then run `npm run dev`.

The PlatformIO build process runs `prebuild-ui.py` which takes the Svelte UI files and bundles them into C++ header files in the `include\ui` folder. It tracks a hash of the UI files (`hash.txt`) so they're not rebuilt unless changes have been made. They are then uploaded along with the rest of the C++ code.

If you want to view the saved JSON sequences on your ESP32, I recommend exploring your ESP32 using the excellent [ESP Connect](https://thelastoutpostworkshop.github.io/ESPConnect/).