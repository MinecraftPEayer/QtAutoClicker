#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
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
  setUiEnabled(false);

  ipcSocket = new QLocalSocket(this);
  reconnectTimer = new QTimer(this);

  QString configPath =
      QApplication::applicationDirPath() + "/autoclicker_config.json";
  QFile configFile(configPath);
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

        ui->hotkeyEdit->setText(hotkeyName);
      }
    } else {
      qWarning() << "Failed to parse config file:" << parseError.errorString();
    }
  } else {
    qWarning() << "Failed to open config file:" << configFile.errorString();
  }

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

void MainWindow::closeEvent(QCloseEvent* event) {
  QString appDir = QApplication::applicationDirPath();
  QDir dir(appDir);
  QString configFilePath = dir.filePath("autoclicker_config.json");

  QJsonObject json;
  json["hotkey"] = hotkey_linux_code;
  json["hotkey_text"] = hotkeyName;

  QJsonDocument doc(json);
  QFile configFile(configFilePath);
  if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    configFile.write(doc.toJson(QJsonDocument::Indented));
    configFile.close();
  } else {
    qWarning() << "Failed to write config file:" << configFile.errorString();
  }

  event->accept();
}

void MainWindow::on_button_question_clicked() {
  QMessageBox msgBox;
  msgBox.setWindowTitle("Info");
  msgBox.setIcon(QMessageBox::Information);
  msgBox.setText(
      "If interval is set to 100 milliseconds and Random\n"
      "offset is set to 40, then the actual value of\n"
      "interval is a random number in the range of 60 to\n"
      "140.");
  msgBox.setOptions(QMessageBox::Option::DontUseNativeDialog);
  msgBox.exec();
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
    QJsonObject dataObj;
    dataObj["command"] = "SET_HOTKEY";

    QJsonObject configObj;
    configObj["hotkey"] = hotkey_linux_code;
    configObj["hotkey_text"] = hotkeyName;

    dataObj["data"] = configObj;
    QJsonDocument doc(dataObj);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
    jsonData.append('\n');
    if (ipcSocket && ipcSocket->state() == QLocalSocket::ConnectedState) {
      ipcSocket->write(jsonData);
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
    ui->StartButton->setEnabled(false);
    ui->StopButton->setEnabled(true);

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
    ui->StartButton->setEnabled(true);
    ui->StopButton->setEnabled(false);

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
