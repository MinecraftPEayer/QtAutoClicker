#ifndef HOTKEYEDIT_H
#define HOTKEYEDIT_H

#include <QKeyEvent>
#include <QLineEdit>

class HotkeyEdit : public QLineEdit {
  Q_OBJECT
 public:
  explicit HotkeyEdit(QWidget* parent = nullptr);

  int getLinuxKeycode() const;

 signals:
  void hotkeyChanged(int newKeycode, const QString& hotkeyText);

 protected:
  void keyPressEvent(QKeyEvent* event) override;

 private:
  int m_linuxKeycode;
};

#endif  // HOTKEYEDIT_H
