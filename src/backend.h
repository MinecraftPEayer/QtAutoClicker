#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <QThread>
#include <QTimer>

#include "evdevworker.h"

#ifndef BACKEND_H
#define BACKEND_H

enum class MouseButton { Left, Right, Middle, Unknown };

class Backend : public QObject {
  Q_OBJECT
 public:
  Backend(QCoreApplication& app);
  ~Backend();
  int exec();

 private:
  QCoreApplication& app;

  QLocalServer* server;
  QLocalSocket* clientSocket;

  QThread* evdevThread;
  EvdevWorker* evdevWorker;

  int uinput_fd;

  MouseButton active_btn;
  int active_btn_linux_code;
  int interval_ms;

  bool clicking = false;

  QTimer* clickTimer;

  void init();
  void initUinputDevice();
  void initTimer();
  void initEvdevWorker();

  void setupSocketServer();
  void handleNewConnection();

  void triggerUinputEvent();
};

#endif  // BACKEND_H
