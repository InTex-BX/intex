#include <memory>
#include <thread>
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <iostream>
#include <tuple>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wdeprecated"
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wpadded"
#include <QObject>
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wdeprecated"
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Wdocumentation-unknown-command"
#pragma clang diagnostic ignored "-Wshift-sign-overflow"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wweak-vtables"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#include <QGst/Init>
#include <QGst/Pipeline>
#include <QGst/ElementFactory>
#include <QGst/Bus>
#include <QGst/Message>
#include <QGst/Event>
#include <QGlib/Connect>
#include <QGst/Element>
#include <QGst/Parse>
#include <QGst/Pad>
#include <QGlib/Connect>
#include <QGlib/Error>

#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wswitch-enum"
#include <boost/asio.hpp>
#pragma clang diagnostic pop

#include <capnp/message.h>
#include <capnp/serialize-packed.h>

#include <gperftools/profiler.h>

#include "intex.capnp.h"

using boost::asio::ip::tcp;

const char pipeline_fmt[] =
    "v4l2src device=/dev/video%1 name=src ! h264parse ! omxh264dec ! "
    "videoscale ! video/x-raw,width=768,height=432,framerate=30/1 ! "
    "omxh264enc name=encoder target_bitrate=%4 control-rate=variable ! "
    "rtph264pay config-interval=1 ! udpsink host=%2 port=%3 sync=false";

class stream : public QObject {
  Q_OBJECT
  QGst::PipelinePtr pipeline;

public:
  stream(QString device, QString host, QString port,
         unsigned bitrate = 400000) {
    auto fmt =
        QString(pipeline_fmt).arg(device, host, port, QString::number(bitrate));
    qCritical() << fmt;
    pipeline = QGst::Parse::launch(fmt).dynamicCast<QGst::Pipeline>();
  }

  ~stream() { pipeline->setState(QGst::StateNull); }

  void start() {
    if (pipeline->setState(QGst::StatePlaying) == QGst::StateChangeFailure) {
      throw std::runtime_error("Unable to change to playing state 0");
    }
    QGlib::connect(pipeline->bus(), "message", this, &stream::onBusMessage);
  }

  void set_stream_bitrate(unsigned bitrate) {
    std::cout << "Setting bitrate: " << bitrate << std::endl;
    pipeline->getElementByName("encoder")
        ->setProperty("target-bitrate", bitrate);
  }

private Q_SLOTS:
  void onBusMessage(const QGst::MessagePtr &message) {
    bool run = message->type() != QGst::MessageError &&
               message->type() != QGst::MessageEos;
    qCritical() << message->typeName();
    switch (message->type()) {
    case QGst::MessageUnknown:
      break;
    case QGst::MessageEos:
      break;
    case QGst::MessageError:
      qCritical() << message.dynamicCast<QGst::ErrorMessage>()->debugMessage();
      break;
    case QGst::MessageWarning:
      qCritical()
          << message.dynamicCast<QGst::WarningMessage>()->debugMessage();
      break;
    case QGst::MessageInfo:
      qCritical() << message.dynamicCast<QGst::InfoMessage>()->debugMessage();
      break;
    case QGst::MessageTag:
      break;
    case QGst::MessageBuffering:
      break;
    case QGst::MessageStateChanged:
      break;
    case QGst::MessageStateDirty:
      break;
    case QGst::MessageStepDone:
      break;
    case QGst::MessageClockProvide:
      break;
    case QGst::MessageClockLost:
      break;
    case QGst::MessageNewClock:
      break;
    case QGst::MessageStructureChange:
      break;
    case QGst::MessageStreamStatus:
      break;
    case QGst::MessageApplication:
      break;
    case QGst::MessageElement:
      break;
    case QGst::MessageSegmentStart:
      break;
    case QGst::MessageSegmentDone:
      break;
    case QGst::MessageDurationChanged:
      break;
    case QGst::MessageLatency:
      break;
    case QGst::MessageAsyncStart:
      break;
    case QGst::MessageAsyncDone:
      break;
    case QGst::MessageRequestState:
      break;
    case QGst::MessageStepStart:
      break;
    case QGst::MessageQos:
      break;
    case QGst::MessageAny:
      break;
    }
  }

};

static std::tuple<QString, QString> read_start(tcp::socket &socket) {
  ::capnp::PackedFdMessageReader reader(socket.native_handle());
  auto start = reader.getRoot<Message>().getStartstream();

  return std::make_tuple(QString::number(start.getPort0()),
                         QString::number(start.getPort1()));
}

static void connection(tcp::socket &socket) {
  std::cout << "Connected: " << socket.remote_endpoint() << std::endl;
  boost::system::error_code error;

  QString port0, port1;
  std::tie(port0, port1) = read_start(socket);

  stream s0(
      QString::number(0),
      QString::fromStdString(socket.remote_endpoint().address().to_string()),
      port0);
/*  stream s1(
      QString::number(1),
      QString::fromStdString(socket.remote_endpoint().address().to_string()),
      port1);
*/
  ProfilerStart("bitrate.prof");
  s0.start();
  //s1.start();

  for (bool stop = false; !stop;) {
    try {
      ::capnp::PackedFdMessageReader reader(socket.native_handle());
      auto bitrate_msg = reader.getRoot<Message>().getChangestream();
      std::cout << "Setting bitrate to " << bitrate_msg.getBitrate()
                << std::endl;
      s0.set_stream_bitrate(bitrate_msg.getBitrate());
    } catch (const kj::Exception &e) {
      std::cerr << e.getDescription().cStr() << std::endl;
      stop = true;
    }
  }
  ProfilerStop();
  ProfilerFlush();
}

int main(int argc, char *argv[]) {
  using namespace std::chrono;
  QGst::init(&argc, &argv);

  boost::asio::io_service io_service;
  tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 31415));

  tcp::socket socket(io_service);
  for (;;) {
    acceptor.accept(socket);
    connection(socket);
    socket.close();
  }
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#include "bitrate.moc"
#pragma clang diagnostic pop
