# QtAutoClicker

A simple auto-clicker application built with Qt and uinput.

## Install

```bash
# use yay
yay -S qtautoclicker-git

# or use paru
paru -S qtautoclicker-git
```

> [!WARNING]
> **Privilege Requirement**: The application utilizes `polkit` to securely request root privileges at runtime, which is necessary to interact with `/dev/uinput` for hardware-level click emulation.

## Build from Source

```bash
git clone https://github.com/MinecraftPEayer/QtAutoClicker.git
cd QtAutoClicker
cmake -S . -B build -D CMAKE_BUILD_TYPE=Release
cmake --build build
```

## Data and Logs

This application stores user configuration (like hotkey) and runtime diagnostics in the following locations:

- **Config file:**  
  Typically under the Qt AppConfigLocation, for example:  
  `~/.config/AutoClicker/autoclicker_config.json`  
  (the exact folder may vary depending on application naming/environment).

- **Backend log file:**  
  `/tmp/autoclicker_backend.log`  
  This file is recreated when the backend starts, so older logs may be overwritten.

- **IPC socket (runtime):**  
  `/tmp/AutoClickerSocket`  
  This is a temporary runtime socket used for frontend/backend communication.
