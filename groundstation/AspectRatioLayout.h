#pragma once

#include <QLayout>
#include <QSize>
#include <QList>
#include <QtGlobal>
#include <QRect>

class AspectRatioLayout : public QLayout {
public:
  explicit AspectRatioLayout(QWidget *parent = nullptr);
  AspectRatioLayout();
  ~AspectRatioLayout();

  void addItem(QLayoutItem *item) Q_DECL_OVERRIDE;
  QLayoutItem *itemAt(int index) const Q_DECL_OVERRIDE;
  QLayoutItem *takeAt(int index) Q_DECL_OVERRIDE;
  QSize minimumSize() const Q_DECL_OVERRIDE;
  void setGeometry(const QRect &rect) Q_DECL_OVERRIDE;
  QSize sizeHint() const Q_DECL_OVERRIDE;
  Qt::Orientations expandingDirections() const Q_DECL_OVERRIDE;
  int count() const Q_DECL_OVERRIDE;

  bool hasHeightForWidth() const;
  int heightForWidth(int width) const;

  void addWidget(QWidget *widget);

private:
  QSize calculateSize(const bool minimumSize = false) const;
  QList<QLayoutItem *> items;
};
