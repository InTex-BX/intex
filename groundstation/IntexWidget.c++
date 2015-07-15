#include <QPainter>
#include <QPaintEvent>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QHBoxLayout>
#include <QLabel>
#include <QSizePolicy>
#include <QVector>
#include <QRect>
#include <QPair>
#include <QTransform>
#include <QPolygon>
#include <QFormLayout>
#include <QMenu>
#include <QAction>
#include <QList>
#include <QPlainTextEdit>

#include <QDebug>

#include <algorithm>
#include <tuple>
#include <iterator>

#include "IntexWidget.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
class PneumaticWidget : public QFrame {
  Q_OBJECT

  QMenu *contextMenu_;
  QList<QAction *> actions;

  void contextMenuEvent(QContextMenuEvent *event) Q_DECL_OVERRIDE {
    if (contextMenu_ != nullptr)
      contextMenu_->popup(event->globalPos());
  }

public:
  enum class Direction { North, East, South, West };

  PneumaticWidget(QWidget *parent = nullptr, QMenu *contextMenu = nullptr)
      : QFrame(parent), contextMenu_(contextMenu) {
    setMinimumSize({20, 40});
  }
  QSize sizeHint() const override { return QSize{32, 64}; }
  QSize widgetSize() const {
    const auto height = size().height() - 2;
    const auto width = size().width() - 2;
    const auto widgetHeight = qMin(height, width * 2);
    return {qMin(width, (widgetHeight + 1) / 2), widgetHeight};
  }

  QAction *addAction(QString text) {
    actions.push_back(contextMenu_->addAction(std::move(text)));
    return actions.back();
  }

public Q_SLOTS:
  void setConnected(bool connected) {
    for (auto &&action : actions) {
      action->setEnabled(connected);
    }
  }

protected:
  enum Direction translateDirection(const enum Direction dir) {
    switch (dir) {
    case Direction::North:
      return Direction::South;
    case Direction::East:
      return Direction::West;
    case Direction::West:
      return Direction::East;
    case Direction::South:
      return Direction::North;
    };
  }

  // clang-format off
Q_SIGNALS:
  void connectionOffset(const enum Direction dir, const int offset);
  // clang-format on
};

class DebugWidget : public PneumaticWidget {
  qreal factor;
  enum Direction direction;

public:
  DebugWidget(const enum Direction dir, qreal factor_)
      : factor(factor_), direction(dir) {}

  void resizeEvent(QResizeEvent *event) override {
    PneumaticWidget::resizeEvent(event);
    int offset;
    switch (direction) {
    case Direction::North:
    case Direction::South:
      offset = static_cast<int>(size().width() * factor);
      break;
    case Direction::East:
    case Direction::West:
      offset = static_cast<int>(size().height() * factor);
      break;
    }
    Q_EMIT connectionOffset(direction, offset);
  }
};

class Tank : public PneumaticWidget {
  Q_OBJECT

  QAction *pressureAction;

public:
  Tank(QWidget *parent = 0)
      : PneumaticWidget(parent, new QMenu),
        pressureAction(addAction(tr("Depressurize"))) {
    pressureAction->setCheckable(true);
    connect(pressureAction, &QAction::triggered, [this](bool checked) {
      Q_EMIT depressurizeRequest(checked, [this, checked](const bool success) {
        pressureAction->setChecked(success ? checked : !checked);
      });
    });
  }

  void paintEvent(QPaintEvent *event) override {
    QFrame::paintEvent(event);
    QPainter painter(this);
    const QSize tankSize = widgetSize();

    painter.translate((size().width() - tankSize.width()) / 2,
                      (size().height() - tankSize.height()) / 2);

    const auto radius = tankSize.width() / 2;
    painter.drawRoundedRect(QRectF{{0, 0}, tankSize}, radius, radius);
    const auto start = QPointF(tankSize.width() / 2, tankSize.height());
    const auto end = QPointF(tankSize.width() / 2, size().height());
    painter.drawLine(start, end);
  }

  void resizeEvent(QResizeEvent *event) override {
    PneumaticWidget::resizeEvent(event);
    Q_EMIT connectionOffset(Direction::South, size().width() / 2);
  }

  // clang-format off
Q_SIGNALS:
  void depressurizeRequest(bool depressurize, std::function<void(bool)> cb);
  // clang-format on
};

class Outlet : public PneumaticWidget {
protected:
  static constexpr auto outletHeightFraction = 0.9;
  static constexpr auto indentWidthFraction = 0.05;

  auto outletHeight() const { return size().height() * outletHeightFraction; }

public:
  Outlet(QWidget *parent = 0, QMenu *contextMenu = nullptr)
      : PneumaticWidget(parent, contextMenu) {}
  void paintEvent(QPaintEvent *event) override {
    QPointF antenna[6];
    QFrame::paintEvent(event);
    QPainter painter(this);

    const auto mid = size().width() * 0.5;
    const auto outletHeight_ = outletHeight();
    const auto indentWidth = size().height() * indentWidthFraction;
    antenna[0] = {mid, outletHeight_};
    antenna[1] = {mid - indentWidth, outletHeight_ - indentWidth};
    antenna[2] = {mid + indentWidth, outletHeight_ - indentWidth};
    antenna[3] = antenna[0];
    antenna[4] = {mid, static_cast<qreal>(size().height())};

    painter.drawPolyline(antenna, 5);
  }
};

class Antenna : public Outlet {
  Q_OBJECT

  QAction *inflateAction;
  QAction *equalizeAction;

  static constexpr auto antennaIndentFraction = 0.1;
  qreal inflateState = 0;

public:
  Antenna(QWidget *parent = 0)
      : Outlet(parent, new QMenu), inflateAction(addAction(tr("Inflate"))),
        equalizeAction(addAction(tr("Equalize"))) {
    inflateAction->setCheckable(true);
    equalizeAction->setCheckable(true);
    connect(inflateAction, &QAction::triggered, [this](bool checked) {
      Q_EMIT inflateRequest(checked, [this, checked](const bool success) {
        inflateAction->setChecked(success ? checked : !checked);
      });
    });
    connect(equalizeAction, &QAction::triggered, [this](bool checked) {
      Q_EMIT equalizeRequest(checked, [ this, checked ](const bool success) {
        equalizeAction->setChecked(success ? checked : !checked);
      });
    });
  }

  void paintEvent(QPaintEvent *event) override {
    QPointF antenna[9];
    Outlet::paintEvent(event);
    QPainter painter(this);

    QSizeF antennaSize = widgetSize();
    painter.translate((size().width() - antennaSize.width()) / 2,
                      size().height() - 1);
    painter.scale(1, -1);

    const auto minHeight = size().height() * (1 - outletHeightFraction) +
                           size().height() * indentWidthFraction + 2;
    antennaSize.setHeight(qMax(antennaSize.height() * inflateState, minHeight));

    const auto indentWidth = antennaSize.width() * antennaIndentFraction;
    const auto indentHeight = antennaSize.height() * 0.6;
    antenna[0] = {0, 0};
    antenna[1] = {antennaSize.width(), 0};
    antenna[2] = {antennaSize.width(), indentHeight};
    antenna[3] = {antennaSize.width() - indentWidth, indentHeight};
    antenna[4] = {antennaSize.width() - indentWidth, antennaSize.height()};
    antenna[5] = {indentWidth, antennaSize.height()};
    antenna[6] = {indentWidth, indentHeight};
    antenna[7] = {0, indentHeight};
    antenna[8] = antenna[0];

    painter.drawPolyline(antenna, 9);
  }

public Q_SLOTS:
  void inflationChanged(qreal inflation) {
    inflateState = inflation;
    update();
  }

  // clang-format off
Q_SIGNALS:
  void inflateRequest(bool inflate, std::function<void(bool)> cb);
  void equalizeRequest(bool equalize, std::function<void(bool)> cb);
  // clang-format on
};

class Connection : public PneumaticWidget {
  Q_OBJECT

protected:
  struct Anchor {
    enum Direction direction;
    int offset;
  };

private:
  QVector<Anchor> anchors;

  qreal angle_;

protected:
  enum Direction normalize(const enum Direction north,
                           const enum Direction dir) const {
    const auto dir_ = static_cast<int>(dir);
    const auto base = static_cast<int>(north);
    const auto result = (dir_ - base + 4) % 4;
    return static_cast<enum Direction>(result);
  }

  const Anchor &reference() const { return anchors.front(); }

  int coordinate(const Anchor &anchor, const int ref) const {
    if (anchor.offset == -1)
      return ref / 2;
    switch (reference().direction) {
    case Direction::North:
    case Direction::East:
      return anchor.offset;
    case Direction::South:
    case Direction::West:
      return ref - anchor.offset;
    }
  }

  std::tuple<QPoint, int, int> rotationAngle() const {
    const int width = size().width();
    const int height = size().height();
    switch (reference().direction) {
    case Direction::North:
      return std::make_tuple(QPoint{0, 0}, width, height);
    case Direction::East:
      return std::make_tuple(QPoint{width, 0}, height, width);
    case Direction::South:
      return std::make_tuple(QPoint{width, height}, width, height);
    case Direction::West:
      return std::make_tuple(QPoint{0, height}, height, width);
    }
  }

  QPoint toPoint(const Anchor &from, int width, int height) const {
    const auto x = coordinate(from, width);
    const auto y = coordinate(from, height);
    switch (normalize(reference().direction, from.direction)) {
    case Direction::North:
      return {x, 0};
    case Direction::East:
      return {width, y};
    case Direction::South:
      return {x, height};
    case Direction::West:
      return {0, y};
    };
  }

  Connection(std::initializer_list<Anchor> anchors_, QWidget *parent = 0)
      : PneumaticWidget(parent), anchors(anchors_),
        angle_(static_cast<int>(anchors.front().direction) * 90.0) {}

public:
  Connection(const enum Direction from, const enum Direction to,
             QWidget *parent = 0)
      : Connection({{from, -1}, {to, -1}}, parent) {}
  Connection(const enum Direction in, const enum Direction out,
             const enum Direction out2, QWidget *parent = 0)
      : Connection({{in, -1}, {out, -1}, {out2, -1}}, parent) {}
  QSize sizeHint() const override { return QSize{20, 40}; }
  void paintEvent(QPaintEvent *event) override {
    QFrame::paintEvent(event);
    QPainter painter(this);

    int width;
    int height;
    QPoint translation;
    std::tie(translation, width, height) = rotationAngle();

    painter.translate(translation);
    painter.rotate(angle_);

    const auto start = QPoint(coordinate(reference(), width), 0);

    std::sort(std::begin(anchors) + 1, std::end(anchors),
              [width, height, this](const auto &lhs, const auto &rhs) {
                return toPoint(lhs, width, height).y() <
                       toPoint(rhs, width, height).y();
              });

    int straightHeight = height / 2;
    if (anchors.size() > 2) {
      const auto length = qMin(qMin(width, height) * 0.25, 32.0);
      straightHeight = toPoint(anchors.at(1), width, height).y();
      const auto crossing = QPoint{start.x(), straightHeight};
      painter.setBrush(Qt::black);
      painter.drawEllipse(QPointF{crossing}, length * 0.1, length * 0.1);
      painter.setBrush(Qt::NoBrush);
      painter.save();
      painter.translate(crossing.x() - length / 2, crossing.y() - length / 2);
      painter.drawRect(QRectF{{0, 0}, QSizeF{length, length}});
      painter.restore();
    }

    std::for_each(std::cbegin(anchors) + 1, std::cend(anchors),
                  [this, &start, &painter, width, height,
                   straightHeight](const auto &to) {

                    const auto end = toPoint(to, width, height);

                    if (normalize(reference().direction, to.direction) !=
                        Direction::South) {
                      const auto midPoint = QPoint{start.x(), end.y()};
                      painter.drawLine(start, midPoint);
                      painter.drawLine(midPoint, end);
                    } else {
                      const auto midPoint1 = QPoint(start.x(), straightHeight);
                      const auto midPoint2 = QPoint(end.x(), straightHeight);

                      painter.drawLine(start, midPoint1);
                      painter.drawLine(midPoint1, midPoint2);
                      painter.drawLine(midPoint2, end);
                    }
                  });
  }

public Q_SLOTS:
  void connectionOffsetChanged(const enum Direction dir, const int offset) {
    auto dir_ = translateDirection(dir);
    for (auto &&anchor : anchors) {
      if (dir_ == anchor.direction)
        anchor.offset = offset;
    }
  }
};

class ElectricValve : public PneumaticWidget {
  Q_OBJECT

  QAction *valveAction;

public:
  ElectricValve(QWidget *parent = 0)
      : PneumaticWidget(parent, new QMenu), valveAction(addAction(tr("Open"))) {
    valveAction->setCheckable(true);
    valveAction->setChecked(false);
    connect(valveAction, &QAction::triggered, this,
            &ElectricValve::valveRequest);
  }

  void paintEvent(QPaintEvent *event) override {
    PneumaticWidget::paintEvent(event);
    QPainter painter(this);

    const auto size_ = widgetSize();
    const auto heightIncrement = size_.height() / 7;
    const auto width = heightIncrement * 2;
    const auto halfWidth = heightIncrement;
    const auto quarterWidth = heightIncrement / 2;

    painter.translate((size().width() - width) / 2,
                      (size().height() - size_.height()) / 2);

    const QSize relaisSize{halfWidth, heightIncrement * 3 / 2};
    const QSize smallRectSize{width, heightIncrement};
    QVector<QRect> rects;

    painter.drawLine(QPoint(0, relaisSize.height() / 3),
                     QPoint(halfWidth, relaisSize.height() * 2 / 3));
    auto height = 0;
    rects.append(QRect({0, height}, relaisSize));
    height += rects.back().height();
    rects.append(QRect({0, height}, smallRectSize));
    height += rects.back().height();
    rects.append(QRect({0, height}, smallRectSize));
    height += rects.back().height();
    rects.append(QRect({0, height}, QSize(width, 2 * heightIncrement)));
    const auto connectionHeight = height + heightIncrement;
    height += rects.back().height();
    painter.drawRects(rects);

    const auto springHeightInc = heightIncrement * 3 / 2 / 6;
    QVector<QLine> spring;
    spring.push_back(
        QLine{quarterWidth, height, halfWidth, height + springHeightInc / 2});
    height += spring.back().dy();
    spring.push_back(QLine{halfWidth, height, 0, height + springHeightInc});
    height += spring.back().dy();
    spring.push_back(QLine{0, height, halfWidth, height + springHeightInc});
    height += spring.back().dy();
    spring.push_back(QLine{halfWidth, height, 0, height + springHeightInc});
    height += spring.back().dy();
    spring.push_back(QLine{0, height, halfWidth, height + springHeightInc});
    height += spring.back().dy();
    spring.push_back(QLine{halfWidth, height, 0, height + springHeightInc});
    height += spring.back().dy();
    spring.push_back(
        QLine{0, height, quarterWidth, height + springHeightInc / 2});

    painter.drawLines(spring);

    const auto connectionTop = connectionHeight - springHeightInc;
    const auto connectionBottom = connectionHeight + springHeightInc;

    /* keep height valid */
    painter.resetTransform();
    painter.translate(0, (size().height() - size_.height()) / 2);
    /* draw connection lines to the border of the widget */
    if (valveAction->isChecked()) {
      const auto leftEnd = size().width() / 2 - quarterWidth;
      const auto rightEnd = size().width() / 2 + quarterWidth;
      painter.drawLine(
          QLine{0, connectionHeight, size().width(), connectionHeight});
      QPolygon arrowHead;
      arrowHead << QPoint(rightEnd, connectionTop);
      arrowHead << QPoint(rightEnd, connectionBottom);
      arrowHead << QPoint(rightEnd + quarterWidth, connectionHeight);
      painter.setBrush(Qt::black);
      painter.drawPolygon(arrowHead);
    } else {
      const auto leftEnd = size().width() / 2 - quarterWidth;
      const auto rightEnd = size().width() / 2 + quarterWidth;
      painter.drawLine(QLine{0, connectionHeight, leftEnd, connectionHeight});
      painter.drawLine(
          QLine{leftEnd, connectionTop, leftEnd, connectionBottom});
      painter.drawLine(
          QLine{rightEnd, connectionHeight, size().width(), connectionHeight});
      painter.drawLine(
          QLine{rightEnd, connectionTop, rightEnd, connectionBottom});
    }
  }

  void resizeEvent(QResizeEvent *event) override {
    PneumaticWidget::resizeEvent(event);

    const auto size_ = widgetSize();
    const auto heightIncrement = size_.height() / 7;
    const auto height = 4 * heightIncrement + heightIncrement / 2 +
                        (size().height() - size_.height()) / 2;

    Q_EMIT connectionOffset(Direction::East, height);
    Q_EMIT connectionOffset(Direction::West, height);
  }

public Q_SLOTS:
  void setState(bool open) {
    valveAction->setChecked(open);
    update();
  }

  // clang-format off
Q_SIGNALS:
  void valveRequest(bool open);
  // clang-format on
};
#pragma clang diagnostic pop

struct IntexWidget::Impl {
  QPlainTextEdit *log;
  ElectricValve * valve1;
  ElectricValve * valve2;

  Impl(QWidget *parent = nullptr)
      : log(new QPlainTextEdit(parent)), valve1(new ElectricValve(parent)),
        valve2(new ElectricValve(parent)) {
    log->setReadOnly(true);
    log->setCenterOnScroll(true);
  }
};

IntexWidget::IntexWidget(QWidget *parent)
    : QFrame(parent), d(std::make_unique<Impl>(this)) {
  setFrameShape(QFrame::StyledPanel);
  setSizePolicy(QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Expanding);
  int row = 0;
  int col = 0;
  auto layout = new QGridLayout(this);
  layout->setRowMinimumHeight(1, 64);
  layout->setSpacing(0);

  auto tank = new Tank;
  auto antenna = new Antenna;
  auto connector1 = new Connection(PneumaticWidget::Direction::North,
                                   PneumaticWidget::Direction::East);
  QObject::connect(tank, &PneumaticWidget::connectionOffset, connector1,
                   &Connection::connectionOffsetChanged);

  QObject::connect(d->valve1, &PneumaticWidget::connectionOffset, connector1,
                   &Connection::connectionOffsetChanged);

  auto yconnector = new Connection(PneumaticWidget::Direction::West,
                                   PneumaticWidget::Direction::North,
                                   PneumaticWidget::Direction::East);
  QObject::connect(d->valve1, &PneumaticWidget::connectionOffset, yconnector,
                   &Connection::connectionOffsetChanged);

  QObject::connect(d->valve2, &PneumaticWidget::connectionOffset, yconnector,
                   &Connection::connectionOffsetChanged);

  auto connector2 = new Connection(PneumaticWidget::Direction::West,
                                   PneumaticWidget::Direction::North);
  QObject::connect(d->valve2, &PneumaticWidget::connectionOffset, connector2,
                   &Connection::connectionOffsetChanged);

  auto internalInfo = new QWidget;
  auto internalInfoLayout = new QFormLayout(internalInfo);
  internalInfoLayout->addRow("Pressure:", new QLabel("8.000 mBar"));
  internalInfoLayout->addRow("Temperature:", new QLabel("36.0 °C"));

  auto externalInfo = new QWidget;
  auto externalInfoLayout = new QFormLayout(externalInfo);
  externalInfoLayout->addRow("Pressure:", new QLabel("42 mBar"));
  externalInfoLayout->addRow("Pressure (Ant):", new QLabel("255 mBar"));
  externalInfoLayout->addRow("Temperature:", new QLabel("-47 °C"));

  layout->addWidget(tank, 0, 0);
  layout->addWidget(internalInfo, 0, 1);
  layout->addWidget(antenna, 0, 2);
  layout->addWidget(externalInfo, 0, 3);
  layout->addWidget(new Outlet, 0, 4);

  layout->addWidget(connector1, 1, 0);
  layout->addWidget(d->valve1, 1, 1);
  layout->addWidget(yconnector, 1, 2);
  layout->addWidget(d->valve2, 1, 3);
  layout->addWidget(connector2, 1, 4);

  layout->addWidget(d->log, 0, 5, 2, 1);

  connect(this, &IntexWidget::onConnectionChanged, tank,
          &PneumaticWidget::setConnected);
  connect(this, &IntexWidget::onConnectionChanged, d->valve1,
          &PneumaticWidget::setConnected);
  connect(this, &IntexWidget::onConnectionChanged, antenna,
          &PneumaticWidget::setConnected);
  connect(this, &IntexWidget::onConnectionChanged, d->valve2,
          &PneumaticWidget::setConnected);

  connect(tank, &Tank::depressurizeRequest, this,
          &IntexWidget::depressurizeRequest);
  connect(d->valve1, &ElectricValve::valveRequest, this,
          &IntexWidget::valve1Request);
  connect(antenna, &Antenna::inflateRequest, this,
          &IntexWidget::inflateRequest);
  connect(d->valve2, &ElectricValve::valveRequest, this,
          &IntexWidget::valve2Request);
}

IntexWidget::~IntexWidget() = default;

void IntexWidget::setPressure(const double pressure) {}

void IntexWidget::setConnected(bool connected) {
  Q_EMIT onConnectionChanged(connected);
}

void IntexWidget::log(QString text) {
  d->log->appendPlainText(std::move(text));
}

void IntexWidget::onValve1Changed(const bool state) {
  d->valve1->setState(state);
}

void IntexWidget::onValve2Changed(const bool state) {
  d->valve2->setState(state);
}

#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "IntexWidget.moc"
#include "moc_IntexWidget.cpp"
