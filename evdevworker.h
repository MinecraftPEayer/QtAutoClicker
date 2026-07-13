#ifndef EVDEVWORKER_H
#define EVDEVWORKER_H

#include <QObject>
#include <QVector>

class EvdevWorker : public QObject {
  Q_OBJECT
 public:
  explicit EvdevWorker(QObject* parent = nullptr);
  ~EvdevWorker();

  void setHotkey(const QVector<int>& hotkey);

 public slots:
  void startPolling();
  void stopPolling();

 signals:
  void hotkeyTriggered();

 private:
  QVector<int> m_hotkey_codes;
  std::atomic<bool> m_stop_requested;
};

#endif  // EVDEVWORKER_H
