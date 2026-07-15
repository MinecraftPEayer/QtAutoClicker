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
