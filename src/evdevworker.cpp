#include "evdevworker.h"

#include <fcntl.h>
#include <linux/input.h>
#include <sys/poll.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QObject>
#include <filesystem>
#include <iostream>
#include <vector>

EvdevWorker::EvdevWorker(QObject* parent)
    : QObject(parent), m_stop_requested(false) {
  m_hotkey_codes.append(KEY_F10);
}

EvdevWorker::~EvdevWorker() { m_stop_requested = true; }

void EvdevWorker::setHotkey(const QVector<int>& hotkey) {
  m_hotkey_codes = hotkey;
}

void EvdevWorker::startPolling() {
  m_stop_requested = false;
  std::vector<int> fds;
  try {
    for (const auto& entry :
         std::filesystem::directory_iterator("/dev/input")) {
      if (entry.path().filename().string().find("event") == 0) {
        int fd = open(entry.path().c_str(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        unsigned long keyBitmask[EV_MAX / 8 + 1] = {0};
        if (ioctl(fd, EVIOCGBIT(0, sizeof(keyBitmask)), keyBitmask) >= 0 &&
            (keyBitmask[0] & (1 << EV_KEY))) {
          fds.push_back(fd);
        } else {
          ::close(fd);
        }
      }
    }
  } catch (...) {
    std::cerr << "Error while scanning /dev/input" << std::endl;
  }

  if (fds.empty()) {
    std::cerr << "No input devices found" << std::endl;
    return;
  }

  std::cout << "[Backend Info] Monitoring input devices for hotkey..."
            << std::endl;

  std::vector<struct pollfd> pollFds(fds.size());

  for (size_t i = 0; i < fds.size(); ++i) {
    pollFds[i].fd = fds[i];
    pollFds[i].events = POLLIN;
  }

  struct input_event ev;

  while (!m_stop_requested) {
    int ret = ::poll(pollFds.data(), pollFds.size(), 100);

    if (ret <= 0) {
      QCoreApplication::processEvents();
      continue;
    }

    for (const auto& pfd : pollFds) {
      if (pfd.revents & POLLIN) {
        while (::read(pfd.fd, &ev, sizeof(ev)) == sizeof(ev)) {
          if (ev.type == EV_KEY && ev.value == 1) {
            if (m_hotkey_codes.contains(ev.code)) {
              std::cout << "[EvdevWorker] Hotkey triggered: " << ev.code
                        << std::endl;
              emit hotkeyTriggered();
            }
          }
        }
      }
    }
    QCoreApplication::processEvents();
  }

  for (int fd : fds) {
    ::close(fd);
  }
}

void EvdevWorker::stopPolling() { m_stop_requested = true; }