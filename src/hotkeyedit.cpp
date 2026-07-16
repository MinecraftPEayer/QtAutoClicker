#include "hotkeyedit.h"

#include <QKeySequence>

HotkeyEdit::HotkeyEdit(QWidget* parent) : QLineEdit(parent), m_linuxKeycode(0) {
  setReadOnly(true);
  setPlaceholderText("Bind");
}

int HotkeyEdit::getLinuxKeycode() const { return m_linuxKeycode; }

void HotkeyEdit::keyPressEvent(QKeyEvent* event) {
  int nativeCode = event->nativeScanCode();

  int evdevKeycode = nativeCode - 8;

  if (evdevKeycode <= 0) return;

  m_linuxKeycode = evdevKeycode;

  this->setText(QKeySequence(event->key()).toString());

  emit hotkeyChanged(m_linuxKeycode, this->text());

  this->clearFocus();
}