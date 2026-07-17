#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QCloseEvent>
#include <QLocalSocket>
#include <QMainWindow>
#include <QString>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  explicit MainWindow(QWidget* parent = nullptr);
  ~MainWindow() override;

 private:
  void closeEvent(QCloseEvent* event) override;

 private slots:
  void on_StartButton_clicked();

  void on_StopButton_clicked();

  void on_input_hours_textEdited(const QString& arg1);

  void on_input_mins_textEdited(const QString& arg1);

  void on_input_secs_textEdited(const QString& arg1);

  void on_input_ms_textEdited(const QString& arg1);

  void on_ButtonComboBox_currentIndexChanged(int index);

 private:
  Ui::MainWindow* ui;
  QLocalSocket* ipcSocket;
  QTimer* reconnectTimer;

  QString hotkeyName;
  int hotkey_linux_code;

  int retryCount;

  void tryConnectToSocket();
  void sendCommand(const QString& command);
  void setUiEnabled(bool enabled);

  int calculateIntervalMs();
  void updateConfig();

  void toggleClickingState(bool clicking);
};
#endif  // MAINWINDOW_H
