#include <QWidget>

#include "AspectRatioLayout.h"

/*
 * AspectRatioLayout lays out Items from left to right, preserving their aspect
 * ratio. Currently, only one Item is supported. The recommended way of using
 * AspectRatioLayout is to have an enclosing widget using AspectRatioLayout.
 * The enclosing widget should have the Preferred size type in both directions.
 * The widget with constant aspect ratio is then added to the
 * AspectRatioLayout.
 *
 * AspectRatioLayout lays out its widgets from left to right, giving each
 * widget equal parts of the total width. The widget is resized to fit the
 * assigned width and height of the AspectRatioLayout while maintaing each
 * widgets aspect ratio. This results in suboptimal layouts if a "slim" widget
 * is positioned beside a "wide" wiget. The slim widget does not need half of
 * the width of the AspectRatioLayout to maintain its aspect ratio, while the
 * wide widget would benefit from the additional width.
 *
 * - LayoutDirections are not supported.  Expanding items are not supported
 *   (because expanding contradicts a fixed aspect ratio). This is the main
 *   reason, only a single Item per AspectRatioLayout is recommended.
 *
 * - Spacing is not considered.
 *
 * - Making widgets visible that were added with addWiget() is probably not
 *   correctly handled. (QLayout::adChildWidget() did not work in all cases.
 *   I'm not familiar enough with the layout system to know how this is
 *   supposed to work. Showing everything suffices for me (Again, only a single
 *   Item per AspectRatioLayout …).
 *
 */

AspectRatioLayout::AspectRatioLayout(QWidget *parent) : QLayout(parent) {}

AspectRatioLayout::AspectRatioLayout() = default;
AspectRatioLayout::~AspectRatioLayout() {
  for (auto &&item : items) {
    delete item;
  }
}

void AspectRatioLayout::addItem(QLayoutItem *item) {
  if (!items.empty()) {
    qWarning("AspectRatioLayout: Attempting to add an Item to a layout which "
             "already has an item. AspectRatioLayout can only lay out a single "
             "Widget. The previous item will be replaced.");
    delete items.takeAt(0);
  }

  items.append(item);
}

void AspectRatioLayout::addWidget(QWidget *widget) {
  // QLayout::addChildWidget(widget);
  QMetaObject::invokeMethod(widget, "_q_showIfNotHidden", Qt::QueuedConnection);
  addItem(new QWidgetItem(widget));
}

Qt::Orientations AspectRatioLayout::expandingDirections() const {
  return Qt::Orientations(0);
}

QLayoutItem *AspectRatioLayout::itemAt(int index) const {
  if (index >= 0 && index < items.size()) {
    return items.at(index);
  }
  return nullptr;
}

QLayoutItem *AspectRatioLayout::takeAt(int index) {
  if (index >= 0 && index < items.size()) {
    return items.takeAt(index);
  }
  return nullptr;
}

int AspectRatioLayout::count() const { return items.size(); }
bool AspectRatioLayout::hasHeightForWidth() const { return true; }
int AspectRatioLayout::heightForWidth(int width) const {
  int height = 0;
  for (auto &&item : items) {
    auto hint = item->sizeHint();
    qreal aspectRatio = 0;
    if (hint.width() && hint.height()) {
      aspectRatio = static_cast<qreal>(hint.width()) / hint.height();
    }
    height = qMax(qRound(width / aspectRatio), height);
  }
  return height;
}
QSize AspectRatioLayout::minimumSize() const { return calculateSize(true); }
QSize AspectRatioLayout::sizeHint() const { return calculateSize(); }

void AspectRatioLayout::setGeometry(const QRect &rect) {
  QLayout::setGeometry(rect);

  if (items.empty())
    return;

  /* TODO:
   * . margins
   * . spacing
   * . spaceritems
   */
  const auto maxWidth = rect.width() / items.count();
  auto width = 0;
  auto height = rect.height();

  for (auto &&item : items) {
    auto hint = item->sizeHint();

    qreal aspectRatio = 0;
    if (hint.width() && hint.height()) {
      aspectRatio = static_cast<qreal>(hint.width()) / hint.height();
    }

    auto currentWidth = qRound(height * aspectRatio);
    if (currentWidth <= maxWidth) {
      /* item is "slim" -> lay out with fixed height */
      auto offset = (maxWidth - currentWidth) / 2;
      QRect layout(rect.x() + width + offset, rect.y(), currentWidth, height);
      item->setGeometry(layout);
      width += maxWidth;
    } else {
      /* item is wide -> lay out with fixed width */
      /* item is wide -> lay out with fixed width */
      auto currentHeight = qRound(maxWidth / aspectRatio);
      auto offset = (height - currentHeight) / 2;
      item->setGeometry(
          QRect(rect.x() + width, rect.y() + offset, maxWidth, currentHeight));
    }
    width += maxWidth;
  }
}

QSize AspectRatioLayout::calculateSize(const bool minimumSize) const {
  QSize totalSize;

  for (const auto &item : items) {
    totalSize += (minimumSize) ? item->minimumSize() : item->sizeHint();
  }
  return totalSize;
}
