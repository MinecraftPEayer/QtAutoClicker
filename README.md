# QtAutoClicker

A simple auto-clicker application built with Qt and uinput.

## Install

Because AUR temporarily disabled new account registration, you can install the application by the following command:

```bash
git clone https://github.com/MinecraftPEayer/QtAutoClicker-AUR.git
cd QtAutoClicker-AUR
makepkg -si
```

> [!WARNING]
> **Privilege Requirement**: The application utilizes `polkit` to securely request root privileges at runtime, which is necessary to interact with `/dev/uinput` for hardware-level click emulation.
