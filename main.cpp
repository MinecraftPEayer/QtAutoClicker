#include <unistd.h>

#include <QApplication>
#include <QCoreApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QString>
#include <iostream>

#include "backend.h"
#include "mainwindow.h"

int runBackend(int argc, char* argv[]) {
  std::freopen("/tmp/autoclicker_backend.log", "a", stdout);
  std::freopen("/tmp/autoclicker_backend.log", "a", stderr);

  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::setvbuf(stderr, nullptr, _IONBF, 0);

  uid_t ruid = getuid();
  uid_t euid = geteuid();

  if (ruid != 0 && euid != 0) {
    return 2;
  }

  clearenv();
  setenv("PATH", "/bin:/usr/bin:/sbin:/usr/sbin", 1);

  QCoreApplication a(argc, argv);
  Backend backend(a);
  return backend.exec();
}

int main(int argc, char* argv[]) {
  bool backendMode = false;
  for (int i = 0; i < argc; i++) {
    if (QString(argv[i]) == "--backend") {
      backendMode = true;
    };
  }

  if (backendMode) {
    return runBackend(argc, argv);
  } else {
    QApplication a(argc, argv);

    QString progPath = QCoreApplication::applicationFilePath();
    QStringList arguments;
    arguments << progPath << "--backend";

    bool success = QProcess::startDetached("pkexec", arguments);

    if (!success) {
      QMessageBox::critical(nullptr, "Error",
                            "Failed to start backend process with pkexec.");
      return 1;
    }

    MainWindow w;
    w.show();
    return a.exec();
  }
}
