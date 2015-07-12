#include <QtGlobal>
#include <QDebug>
#include <QEvent>
#include <QMouseEvent>
#include <QKeySequence>

#include "VideoWindow.h"

VideoWindow::VideoWindow(QWidget *parent)
    : QMainWindow(parent), widget_(new QGst::Ui::VideoWidget(this)),
      close(QKeySequence::Close, this, SLOT(close())) {
  QPalette palette_ = palette();
  palette_.setColor(backgroundRole(), Qt::black);
  setPalette(palette_);
  setAutoFillBackground(true);
  setCentralWidget(widget_);
  widget_->installEventFilter(this);
  close.setAutoRepeat(false);
}

void VideoWindow::mouseMoveEvent(QMouseEvent *event) {
  auto diff = event->pos() - currentPos;
  auto newpos = pos() + diff;

  move(newpos);
}

bool VideoWindow::eventFilter(QObject *obj, QEvent *event) {
  QMouseEvent *e = dynamic_cast<QMouseEvent *>(event);
  if (obj == widget_ && e) {
    if (event->type() == QEvent::MouseButtonDblClick) {
      if (isMaximized()) {
        showNormal();
      } else {
        showMaximized();
      }
    } else if (event->type() == QEvent::MouseButtonPress) {
      currentPos = e->pos();
    }
  }

  return QMainWindow::eventFilter(obj, event);
}

#include "moc_VideoWindow.cpp"
