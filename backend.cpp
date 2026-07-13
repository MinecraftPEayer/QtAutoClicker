#include "backend.h"

#include <fcntl.h>
#include <linux/uinput.h>
#include <sys/stat.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMetaObject>
#include <iostream>

Backend::Backend(QCoreApplication& application)
    : QObject(),
      app(application),
      uinput_fd(-1),
      server(nullptr),
      interval_ms(100),
      active_btn_linux_code(BTN_LEFT) {}

void Backend::init() {
  uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

  if (uinput_fd < 0) {
    std::cerr << "[Fatal] Failed to open /dev/uinput\n";
    app.exit(3);
    return;
  }

  if (ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY) < 0) {
    std::cerr << "[Fatal] Failed to set EV_KEY\n";
    close(uinput_fd);
    uinput_fd = -1;
    app.exit(3);
    return;
  }

  if (ioctl(uinput_fd, UI_SET_KEYBIT, BTN_LEFT) < 0 ||
      ioctl(uinput_fd, UI_SET_KEYBIT, BTN_RIGHT) < 0 ||
      ioctl(uinput_fd, UI_SET_KEYBIT, BTN_MIDDLE) < 0) {
    std::cerr << "[Fatal] Failed to set one or more mouse buttons\n";
    close(uinput_fd);
    uinput_fd = -1;
    app.exit(3);
    return;
  }

  struct uinput_user_dev uidev;
  std::memset(&uidev, 0, sizeof(uidev));
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "Virtual AutoClicker Mouse");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1234;
  uidev.id.product = 0x5678;

  if (write(uinput_fd, &uidev, sizeof(uidev)) < 0) {
    std::cerr << "[Fatal] Failed to write uinput_user_dev\n";
    close(uinput_fd);
    uinput_fd = -1;
    app.exit(3);
    return;
  }

  if (ioctl(uinput_fd, UI_DEV_CREATE) < 0) {
    std::cerr << "[Fatal] Failed to create uinput device\n";
    close(uinput_fd);
    uinput_fd = -1;
    app.exit(3);
    return;
  }

  std::cout << "[Info] Successfully initialized uinput device\n";

  clickTimer = new QTimer(this);
  clickTimer->setTimerType(Qt::PreciseTimer);
  connect(clickTimer, &QTimer::timeout, this, &Backend::triggerUinputEvent);

  evdevThread = new QThread(this);
  evdevWorker = new EvdevWorker();

  evdevWorker->moveToThread(evdevThread);
  connect(evdevThread, &QThread::started, evdevWorker,
          &EvdevWorker::startPolling);
  connect(evdevThread, &QThread::finished, evdevWorker,
          &EvdevWorker::deleteLater);
  evdevThread->start();

  connect(
      evdevWorker, &EvdevWorker::hotkeyTriggered, this,
      [this]() {
        std::cout << "test" << std::endl;
        if (clicking) {
          QMetaObject::invokeMethod(clickTimer, "stop", Qt::QueuedConnection);
          clicking = false;
        } else {
          QMetaObject::invokeMethod(clickTimer, "start", Qt::QueuedConnection,
                                    Q_ARG(int, interval_ms));
          clicking = true;
        }
      },
      Qt::QueuedConnection);

  std::cout << "[Info] Evdev worker thread started\n";
}

int Backend::exec() {
  init();
  setupSocketServer();
  return app.exec();
}

void Backend::setupSocketServer() {
  server = new QLocalServer(this);
  QString serverName = "AutoClickerSocket";

  QLocalServer::removeServer(serverName);

  if (!server->listen(serverName)) {
    std::cerr << "[Fatal] Failed to start socket server: "
              << server->errorString().toStdString() << "\n";
    app.exit(4);
    return;
  }

  QString socketPath = "/tmp/" + serverName;
  if (::chmod(socketPath.toUtf8().constData(), 0666) < 0) {
    std::cerr << "[Backend Warning] Failed to chmod socket file.\n";
  }

  connect(server, &QLocalServer::newConnection, this,
          &Backend::handleNewConnection);
  std::cout << "[Backend Info] Socket server started, listening on: "
            << serverName.toStdString() << "\n";
}

void Backend::handleNewConnection() {
  QLocalSocket* clientSocket = server->nextPendingConnection();

  std::cout << "[Backend Info] New client connected.\n";

  connect(clientSocket, &QLocalSocket::readyRead, this, [this, clientSocket]() {
    while (clientSocket->canReadLine()) {
      QByteArray line = clientSocket->readLine().trimmed();
      if (line.isEmpty()) continue;

      std::cout << "[Backend Info] Received command: " << line.toStdString()
                << "\n";

      QJsonParseError error;
      QJsonDocument doc = QJsonDocument::fromJson(line, &error);

      if (error.error != QJsonParseError::NoError) {
        std::cerr << "[Backend Error] Failed to parse JSON command: "
                  << error.errorString().toStdString() << "\n";
        continue;
      }

      QJsonObject jsonObj = doc.object();
      QString cmd = jsonObj["command"].toString();

      if (cmd == "START") {
        if (!jsonObj.contains("data")) {
          std::cout << "[Backend Warning] START command missing 'data' "
                       "field.\n";
          continue;
        }

        QJsonObject dataObj = jsonObj["data"].toObject();
        if (dataObj.isEmpty()) {
          std::cout << "[Backend Warning] START command 'data' field is "
                       "empty.\n";
          continue;
        }

        if (!dataObj.contains("button") || !dataObj.contains("interval")) {
          std::cout << "[Backend Warning] START command 'data' missing "
                       "required fields.\n";
          continue;
        }

        QString button = dataObj["button"].toString("LEFT").toUpper();
        active_btn = MouseButton::Unknown;
        if (button == "LEFT") {
          active_btn = MouseButton::Left;
        } else if (button == "RIGHT") {
          active_btn = MouseButton::Right;
        } else if (button == "MIDDLE") {
          active_btn = MouseButton::Middle;
        } else {
          std::cout << "[Backend Warning] Invalid button value: "
                    << button.toStdString() << "\n";
          continue;
        }

        active_btn_linux_code = BTN_LEFT;
        switch (active_btn) {
          case MouseButton::Left:
            active_btn_linux_code = BTN_LEFT;
            break;
          case MouseButton::Right:
            active_btn_linux_code = BTN_RIGHT;
            break;
          case MouseButton::Middle:
            active_btn_linux_code = BTN_MIDDLE;
            break;
          default:
            std::cout << "[Backend Warning] Unknown button type.\n";
            continue;
        }

        interval_ms = dataObj["interval"].toInt();
        if (interval_ms < 1) interval_ms = 1;

        clickTimer->start(interval_ms);
      } else if (cmd == "STOP") {
        clickTimer->stop();
      } else if (cmd == "SET_HOTKEY") {
        if (!jsonObj.contains("data")) {
          std::cout << "[Backend Warning] SET_HOTKEY command missing 'data' "
                       "field.\n";
          continue;
        }

        QJsonObject dataObj = jsonObj["data"].toObject();
        if (dataObj.isEmpty()) {
          std::cout << "[Backend Warning] SET_HOTKEY command 'data' field is "
                       "empty.\n";
          continue;
        }

        if (!dataObj.contains("hotkey")) {
          std::cout << "[Backend Warning] SET_HOTKEY command 'data' missing "
                       "'hotkey' field.\n";
          continue;
        }

        int hotkeyCode = dataObj["hotkey"].toInt();

        evdevWorker->setHotkey(QVector<int>() << hotkeyCode);
      } else if (cmd == "UPDATE_CONFIG") {  // button, interval
        if (!jsonObj.contains("data")) {
          std::cout << "[Backend Warning] SET_HOTKEY command missing 'data' "
                       "field.\n";
          continue;
        }

        QJsonObject dataObj = jsonObj["data"].toObject();
        if (dataObj.contains("button") && dataObj["button"].isString()) {
          QString button = dataObj["button"].toString("LEFT").toUpper();
          active_btn = MouseButton::Unknown;
          if (button == "LEFT") {
            active_btn = MouseButton::Left;
          } else if (button == "RIGHT") {
            active_btn = MouseButton::Right;
          } else if (button == "MIDDLE") {
            active_btn = MouseButton::Middle;
          } else {
            std::cout << "[Backend Warning] Invalid button value: "
                      << button.toStdString() << "\n";
            continue;
          }

          active_btn_linux_code = BTN_LEFT;
          switch (active_btn) {
            case MouseButton::Left:
              active_btn_linux_code = BTN_LEFT;
              break;
            case MouseButton::Right:
              active_btn_linux_code = BTN_RIGHT;
              break;
            case MouseButton::Middle:
              active_btn_linux_code = BTN_MIDDLE;
              break;
            default:
              std::cout << "[Backend Warning] Unknown button type.\n";
              continue;
          }
        }

        if (dataObj.contains("interval") && dataObj["interval"].isDouble()) {
          interval_ms = dataObj["interval"].toInt();
          if (interval_ms < 1) interval_ms = 1;
        }
      } else {
        std::cout << "[Backend Warning] Unknown command: " << cmd.toStdString()
                  << "\n";
      }
    }
  });

  connect(clientSocket, &QLocalSocket::disconnected, this,
          [this, clientSocket]() {
            std::cout << "[Backend Info] Frontend client disconnected. Exiting "
                         "backend safely...\n";

            clientSocket->deleteLater();

            app.quit();
          });

  connect(clientSocket, &QLocalSocket::disconnected, clientSocket,
          &QLocalSocket::deleteLater);
  std::cout << "[Backend Info] Frontend client connected.\n";
}

void Backend::triggerUinputEvent() {
  if (uinput_fd < 0) {
    std::cerr << "[Backend Error] uinput device not initialized.\n";
    return;
  }

  struct input_event ev;

  // Press event
  std::memset(&ev, 0, sizeof(ev));
  ev.type = EV_KEY;
  ev.code = active_btn_linux_code;
  ev.value = 1;  // Press
  if (write(uinput_fd, &ev, sizeof(ev)) < 0) {
    std::cerr << "[Backend Error] Failed to write press event.\n";
    return;
  }

  // Synchronize
  std::memset(&ev, 0, sizeof(ev));
  ev.type = EV_SYN;
  ev.code = SYN_REPORT;
  ev.value = 0;
  if (write(uinput_fd, &ev, sizeof(ev)) < 0) {
    std::cerr << "[Backend Error] Failed to write sync event after press.\n";
    return;
  }

  // Release event
  std::memset(&ev, 0, sizeof(ev));
  ev.type = EV_KEY;
  ev.code = active_btn_linux_code;
  ev.value = 0;  // Release
  if (write(uinput_fd, &ev, sizeof(ev)) < 0) {
    std::cerr << "[Backend Error] Failed to write release event.\n";
    return;
  }

  // Synchronize
  std::memset(&ev, 0, sizeof(ev));
  ev.type = EV_SYN;
  ev.code = SYN_REPORT;
  ev.value = 0;
  if (write(uinput_fd, &ev, sizeof(ev)) < 0) {
    std::cerr << "[Backend Error] Failed to write sync event after release.\n";
    return;
  }
}

Backend::~Backend() {
  if (clickTimer) {
    clickTimer->stop();
  }

  if (server) {
    server->close();
  }

  if (uinput_fd >= 0) {
    ioctl(uinput_fd, UI_DEV_DESTROY);
    ::close(uinput_fd);
  }

  evdevWorker->stopPolling();
  evdevThread->quit();
  evdevThread->wait();
}