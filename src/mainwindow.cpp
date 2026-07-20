#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <iostream>

#include "hotkeyedit.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow),
      ipcSocket(nullptr),
      retryCount(0) {
  ui->setupUi(this);

  this->setFixedSize(this->width(), this->height());

  setWindowIcon(QIcon(":/QtAutoClicker.png"));

  setUiEnabled(false);

  ipcSocket = new QLocalSocket(this);
  reconnectTimer = new QTimer(this);

  loadConfig();

  connect(ipcSocket, &QLocalSocket::readyRead, this, [this]() {
    QByteArray data = ipcSocket->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
      QJsonObject obj = doc.object();
      if (obj.contains("command") && obj["command"].isString()) {
        QString command = obj["command"].toString();
        if (command == "CLICK_STARTED") {
          toggleClickingState(true);
        } else if (command == "CLICK_STOPPED") {
          toggleClickingState(false);
        }
      }
    }
  });

  connect(reconnectTimer, &QTimer::timeout, this,
          &MainWindow::tryConnectToSocket);

  connect(
      ui->hotkeyEdit, &HotkeyEdit::hotkeyChanged, this,
      [this](int newKeycode, const QString& hotkeyText) {
        hotkey_linux_code = newKeycode;
        hotkeyName = hotkeyText;

        QJsonObject json;
        json["command"] = "SET_HOTKEY";
        QJsonObject data;
        data["hotkey"] = newKeycode;
        data["hotkey_text"] = hotkeyText;
        json["data"] = data;

        QJsonDocument doc(json);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
        jsonData.append('\n');
        if (ipcSocket && ipcSocket->state() == QLocalSocket::ConnectedState) {
          ipcSocket->write(jsonData);
          ipcSocket->flush();
        }
      });

  reconnectTimer->start(1000);
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::loadConfig() {
  QString configDir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QDir().mkpath(configDir);
  QString configPath = configDir + "/autoclicker_config.json";
  QFile configFile(configPath);

  hotkey_linux_code = 68;
  hotkeyName = "F10";

  int interval_hours = 0, interval_mins = 0, interval_secs = 0,
      interval_ms = 100;

  QString active_button = "Left";

  if (configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QByteArray configData = configFile.readAll();
    configFile.close();
    QJsonParseError parseError;
    QJsonDocument configDoc = QJsonDocument::fromJson(configData, &parseError);
    if (parseError.error == QJsonParseError::NoError && configDoc.isObject()) {
      QJsonObject configObj = configDoc.object();
      if (configObj.contains("hotkey") && configObj["hotkey"].isDouble() &&
          configObj["hotkey_text"].isString()) {
        hotkey_linux_code = configObj["hotkey"].toInt();
        hotkeyName = configObj["hotkey_text"].toString();
      }

      if (configObj.contains("interval") && configObj["interval"].isObject()) {
        QJsonObject intervalObj = configObj["interval"].toObject();
        if (intervalObj.contains("hours") && intervalObj["hours"].isDouble()) {
          interval_hours = intervalObj["hours"].toInt();
        }
        if (intervalObj.contains("mins") && intervalObj["mins"].isDouble()) {
          interval_mins = intervalObj["mins"].toInt();
        }
        if (intervalObj.contains("secs") && intervalObj["secs"].isDouble()) {
          interval_secs = intervalObj["secs"].toInt();
        }
        if (intervalObj.contains("ms") && intervalObj["ms"].isDouble()) {
          interval_ms = intervalObj["ms"].toInt();
        }
      }

      if (configObj.contains("button") && configObj["button"].isString()) {
        QString button = configObj["button"].toString();

        QStringList validButtons = {"Left", "Right", "Middle"};
        if (validButtons.contains(button)) {
          active_button = button;
        } else
          active_button = "Left";
      }
    } else {
      qWarning() << "Failed to parse config file:" << parseError.errorString();
    }
  } else {
    qWarning() << "Failed to open config file:" << configFile.errorString();
  }

  ui->input_hours->setText(QString::number(interval_hours));
  ui->input_mins->setText(QString::number(interval_mins));
  ui->input_secs->setText(QString::number(interval_secs));
  ui->input_ms->setText(QString::number(interval_ms));

  ui->ButtonComboBox->setCurrentText(active_button);

  ui->hotkeyEdit->setText(hotkeyName);
}

void MainWindow::saveConfig() {
  QString configDir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QDir().mkpath(configDir);
  QString configFilePath = configDir + "/autoclicker_config.json";

  QJsonObject json;
  json["hotkey"] = hotkey_linux_code;
  json["hotkey_text"] = hotkeyName;

  QJsonObject intervalObj;
  intervalObj["hours"] = ui->input_hours->text().toInt();
  intervalObj["mins"] = ui->input_mins->text().toInt();
  intervalObj["secs"] = ui->input_secs->text().toInt();
  intervalObj["ms"] = ui->input_ms->text().toInt();
  json["interval"] = intervalObj;

  json["button"] = ui->ButtonComboBox->currentText();

  QJsonDocument doc(json);
  QFile configFile(configFilePath);
  if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    configFile.write(doc.toJson(QJsonDocument::Indented));
    configFile.close();
  } else {
    qWarning() << "Failed to write config file:" << configFile.errorString();
  }
}

void MainWindow::closeEvent(QCloseEvent* event) {
  saveConfig();

  event->accept();
}

void MainWindow::setUiEnabled(bool enabled) {
  ui->StartButton->setEnabled(enabled);
  ui->StopButton->setEnabled(enabled);
}

void MainWindow::tryConnectToSocket() {
  retryCount++;

  ipcSocket->connectToServer("AutoClickerSocket");

  if (ipcSocket->waitForConnected(1000)) {
    setUiEnabled(true);

    toggleClickingState(false);

    QJsonObject dataObj;
    QJsonObject preConfigObj;
    dataObj["command"] = "SET_HOTKEY";
    preConfigObj["command"] = "UPDATE_CONFIG";

    QJsonObject configObj;
    configObj["hotkey"] = hotkey_linux_code;
    configObj["hotkey_text"] = hotkeyName;

    QJsonObject preConfigDataObj;
    preConfigDataObj["interval"] = calculateIntervalMs();
    preConfigDataObj["button"] = ui->ButtonComboBox->currentText().toUpper();

    dataObj["data"] = configObj;
    QJsonDocument doc(dataObj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    jsonData.append('\n');

    preConfigObj["data"] = preConfigDataObj;
    QJsonDocument preConfigDoc(preConfigObj);
    QByteArray preConfigJsonData = preConfigDoc.toJson(QJsonDocument::Compact);
    preConfigJsonData.append('\n');

    if (ipcSocket && ipcSocket->state() == QLocalSocket::ConnectedState) {
      ipcSocket->write(jsonData);
      ipcSocket->flush();
      ipcSocket->write(preConfigJsonData);
      ipcSocket->flush();
    }

    reconnectTimer->stop();
  } else {
    if (retryCount >= 30) {
      QMessageBox::critical(this, "Error",
                            "Failed to connect to backend process.");
      QApplication::quit();
    }
    setUiEnabled(false);
  }
}

void MainWindow::sendCommand(const QString& command) {
  if (!ipcSocket || ipcSocket->state() != QLocalSocket::ConnectedState) return;

  QJsonObject json;
  json["command"] = command;

  if (command == "START") {
    toggleClickingState(true);

    QJsonObject data;
    int interval = ui->input_ms->text().toInt() +
                   ui->input_secs->text().toInt() * 1000 +
                   ui->input_mins->text().toInt() * 60 * 1000 +
                   ui->input_hours->text().toInt() * 60 * 60 * 1000;
    if (interval < 1) interval = 1;
    data["interval"] = interval;

    QString button;
    button = ui->ButtonComboBox->currentText().toUpper();
    if (button == "LEFT") {
      data["button"] = "LEFT";
    } else if (button == "RIGHT") {
      data["button"] = "RIGHT";
    } else if (button == "MIDDLE") {
      data["button"] = "MIDDLE";
    } else {
      data["button"] = "LEFT";
    }

    json["data"] = data;

    QJsonDocument doc(json);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    jsonData.append('\n');
    ipcSocket->write(jsonData);
    ipcSocket->flush();
  } else if (command == "STOP") {
    toggleClickingState(false);

    QJsonDocument doc(json);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    jsonData.append('\n');
    ipcSocket->write(jsonData);
    ipcSocket->flush();
  }
}
void MainWindow::on_StartButton_clicked() { sendCommand("START"); }

void MainWindow::on_StopButton_clicked() { sendCommand("STOP"); }

int MainWindow::calculateIntervalMs() {
  int interval = ui->input_ms->text().toInt() +
                 ui->input_secs->text().toInt() * 1000 +
                 ui->input_mins->text().toInt() * 60 * 1000 +
                 ui->input_hours->text().toInt() * 60 * 60 * 1000;
  if (interval < 1) interval = 1;
  return interval;
}

void MainWindow::updateConfig() {
  int interval = calculateIntervalMs();
  QString button = ui->ButtonComboBox->currentText().toUpper();

  QJsonObject rootJson;
  rootJson["command"] = "UPDATE_CONFIG";
  QJsonObject dataJson;
  dataJson["interval"] = interval;
  dataJson["button"] = button;
  rootJson["data"] = dataJson;
  QJsonDocument doc(rootJson);
  QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
  jsonData.append('\n');

  if (ipcSocket && ipcSocket->state() == QLocalSocket::ConnectedState) {
    ipcSocket->write(jsonData);
    ipcSocket->flush();
  }
}

void MainWindow::on_input_hours_textEdited(const QString& arg1) {
  updateConfig();
}

void MainWindow::on_input_mins_textEdited(const QString& arg1) {
  updateConfig();
}

void MainWindow::on_input_secs_textEdited(const QString& arg1) {
  updateConfig();
}

void MainWindow::on_input_ms_textEdited(const QString& arg1) { updateConfig(); }

void MainWindow::on_ButtonComboBox_currentIndexChanged(int index) {
  updateConfig();
}

void MainWindow::toggleClickingState(bool clicking) {
  if (clicking) {
    ui->StartButton->setEnabled(false);
    ui->StopButton->setEnabled(true);
    ui->StopButton->setFocus();

    MainWindow::setWindowTitle("Auto Clicker - Clicking");
  } else {
    ui->StartButton->setEnabled(true);
    ui->StopButton->setEnabled(false);
    ui->StartButton->setFocus();

    MainWindow::setWindowTitle("Auto Clicker - Stopped");
  }
}