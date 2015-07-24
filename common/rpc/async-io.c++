// Copyright (c) 2014 Sandstorm Development Group, Inc. and contributors
// Licensed under the MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include <errno.h>
#include <unistd.h>
#include <unordered_map>
#include <inttypes.h>
#include <set>
#include <stdlib.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>

#include <typeinfo>
#include <typeindex>
#include <cxxabi.h>

#include <capnp/rpc-twoparty.h>
#include <capnp/rpc.capnp.h>
#include <capnp/serialize.h>
#include <capnp/dynamic.h>
#include <capnp/schema-parser.h>

#include <kj/debug.h>
#include <kj/vector.h>

#include <QCoreApplication>
#include <QDebug>
#include <QObject>
#include <QSocketNotifier>
#include <QTimer>

#include "async-io.h"

#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wgnu-statement-expression"
#pragma clang diagnostic ignored "-Wweak-vtables"

namespace intex {
namespace rpc {

class QtEventPort : public QObject, public kj::EventPort {
  Q_OBJECT

public:
  explicit QtEventPort() : kjLoop(*this) {}
  ~QtEventPort() noexcept {
    while (scheduled) {
      wait();
    }
  }

  kj::EventLoop &getKjLoop() { return kjLoop; }

  bool wait() override {
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
    return false;
  }

  bool poll() override {
    QCoreApplication::processEvents();
    return false;
  }

  void setRunnable(bool runnable) override {
    if (runnable != runnable_) {
      runnable_ = runnable;
      if (runnable_ && !scheduled) {
        schedule();
      }
    }
  }

private:
  kj::EventLoop kjLoop;
  bool runnable_ = false;
  bool scheduled = false;

  void schedule() {
    QTimer::singleShot(0, this, &QtEventPort::run);
    scheduled = true;
  }

  void run() {
    KJ_ASSERT(scheduled);

    if (runnable_) {
      kjLoop.run();
    }

    scheduled = false;

    if (runnable_) {
      // Apparently either we never became non-runnable, or we did but then
      // became runnable again.  Since `scheduled` has been true the whole
      // time, we won't have been rescheduled, so do that now.
      schedule();
    } else {
      scheduled = false;
    }
  }
};

static void setNonblocking(int fd) {
  int flags;
  KJ_SYSCALL(flags = fcntl(fd, F_GETFL));
  if ((flags & O_NONBLOCK) == 0) {
    KJ_SYSCALL(fcntl(fd, F_SETFL, flags | O_NONBLOCK));
  }
}

static void setCloseOnExec(int fd) {
  int flags;
  KJ_SYSCALL(flags = fcntl(fd, F_GETFD));
  if ((flags & FD_CLOEXEC) == 0) {
    KJ_SYSCALL(fcntl(fd, F_SETFD, flags | FD_CLOEXEC));
  }
}

static constexpr uint NEW_FD_FLAGS =
#ifdef __linux__
    kj::LowLevelAsyncIoProvider::ALREADY_CLOEXEC ||
    kj::LowLevelAsyncIoProvider::ALREADY_NONBLOCK ||
#endif
    kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP;
// We always try to open FDs with CLOEXEC and NONBLOCK already set on Linux, but
// on other platforms
// this is not possible.

class OwnedFileDescriptor {
public:
  OwnedFileDescriptor(int fd, uint flags)
      : readNotifier(fd, QSocketNotifier::Read),
        writeNotifier(fd, QSocketNotifier::Write),
        exceptionNotifier(fd, QSocketNotifier::Exception), flags(flags) {
    readNotifier.setEnabled(false);
    writeNotifier.setEnabled(false);

    if (flags & kj::LowLevelAsyncIoProvider::ALREADY_NONBLOCK) {
      KJ_DREQUIRE(fcntl(fd, F_GETFL) & O_NONBLOCK,
                  "You claimed you set NONBLOCK, but you didn't.");
    } else {
      setNonblocking(fd);
    }

    if (flags & kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP) {
      if (flags & kj::LowLevelAsyncIoProvider::ALREADY_CLOEXEC) {
        KJ_DREQUIRE(fcntl(fd, F_GETFD) & FD_CLOEXEC,
                    "You claimed you set CLOEXEC, but you didn't.");
      } else {
        setCloseOnExec(fd);
      }
    }
    QObject::connect(&readNotifier, &QSocketNotifier::activated, [this](int) {
      KJ_ASSERT_NONNULL(readable)->fulfill();
      readNotifier.setEnabled(false);
      readable = nullptr;
    });
    QObject::connect(&writeNotifier, &QSocketNotifier::activated, [this](int) {
      KJ_ASSERT_NONNULL(writable)->fulfill();
      writeNotifier.setEnabled(false);
      writable = nullptr;
    });
    QObject::connect(&exceptionNotifier, &QSocketNotifier::activated,
                     [this](int) {
                       //  Instead of throwing an exception, we'd rather report
                       //  that the fd is now readable/writable and let the
                       //  caller discover the error when they actually attempt
                       //  to read/write.
                       readNotifier.setEnabled(false);
                       writeNotifier.setEnabled(false);
                       KJ_IF_MAYBE(r, readable) {
                         r->get()->fulfill();
                         readable = nullptr;
                       }
                       KJ_IF_MAYBE(w, writable) {
                         w->get()->fulfill();
                         writable = nullptr;
                       }
                     });
  }

  ~OwnedFileDescriptor() {
    readNotifier.setEnabled(false);
    writeNotifier.setEnabled(false);
    exceptionNotifier.setEnabled(false);
    // Don't use KJ_SYSCALL() here because close() should not be repeated on
    // EINTR.
    const int fd = static_cast<int>(readNotifier.socket());
    if ((flags & kj::LowLevelAsyncIoProvider::TAKE_OWNERSHIP) &&
        close(fd) < 0) {
      qDebug() << "Closing FD" << fd << "failed (" << errno << ")"
               << strerror(errno) << ".";
    }
  }

  kj::Promise<void> onReadable() {
    KJ_REQUIRE(readable == nullptr,
               "Must wait for previous event to complete.");

    auto paf = kj::newPromiseAndFulfiller<void>();
    readable = kj::mv(paf.fulfiller);

    readNotifier.setEnabled(true);

    return kj::mv(paf.promise);
  }

  kj::Promise<void> onWritable() {
    KJ_REQUIRE(writable == nullptr,
               "Must wait for previous event to complete.");

    auto paf = kj::newPromiseAndFulfiller<void>();
    writable = kj::mv(paf.fulfiller);

    writeNotifier.setEnabled(true);

    return kj::mv(paf.promise);
  }

protected:
  QSocketNotifier readNotifier;
  QSocketNotifier writeNotifier;
  QSocketNotifier exceptionNotifier;

  int readFd() const { return static_cast<int>(readNotifier.socket()); }
  int writeFd() const { return static_cast<int>(writeNotifier.socket()); }

private:
  uint flags;
  kj::Maybe<kj::Own<kj::PromiseFulfiller<void>>> readable;
  kj::Maybe<kj::Own<kj::PromiseFulfiller<void>>> writable;
};

class QtIoStream : public OwnedFileDescriptor, public kj::AsyncIoStream {
public:
  QtIoStream(int fd, uint flags) : OwnedFileDescriptor(fd, flags) {}
  virtual ~QtIoStream() = default;

  kj::Promise<size_t> read(void *buffer, size_t minBytes,
                           size_t maxBytes) override {
    return tryReadInternal(buffer, minBytes, maxBytes, 0)
        .then([=](size_t result) {
          KJ_REQUIRE(result >= minBytes, "Premature EOF") {
            // Pretend we read zeros from the input.
            memset(reinterpret_cast<capnp::byte *>(buffer) + result, 0,
                   minBytes - result);
            return minBytes;
          }
          return result;
        });
  }

  kj::Promise<size_t> tryRead(void *buffer, size_t minBytes,
                              size_t maxBytes) override {
    return tryReadInternal(buffer, minBytes, maxBytes, 0);
  }

  kj::Promise<void> write(const void *buffer, size_t size) override {
    ssize_t writeResult;
    KJ_NONBLOCKING_SYSCALL(writeResult = ::write(writeFd(), buffer, size)) {
      return kj::READY_NOW;
    }

    // A negative result means EAGAIN, which we can treat the same as having
    // written zero bytes.
    size_t n = writeResult < 0 ? 0 : static_cast<size_t>(writeResult);

    if (n == size) {
      return kj::READY_NOW;
    } else {
      buffer = reinterpret_cast<const capnp::byte *>(buffer) + n;
      size -= n;
    }

    return onWritable().then([=]() { return write(buffer, size); });
  }

  kj::Promise<void>
  write(kj::ArrayPtr<const kj::ArrayPtr<const capnp::byte>> pieces) override {
    if (pieces.size() == 0) {
      return writeInternal(nullptr, nullptr);
    } else {
      return writeInternal(pieces[0], pieces.slice(1, pieces.size()));
    }
  }

  void shutdownWrite() override {
    // There's no legitimate way to get an AsyncStreamFd that isn't a socket
    // through the
    // UnixAsyncIoProvider interface.
    KJ_SYSCALL(shutdown(writeFd(), SHUT_WR));
  }

private:
  kj::Promise<size_t> tryReadInternal(void *buffer, size_t minBytes,
                                      size_t maxBytes, size_t alreadyRead) {
    // `alreadyRead` is the number of bytes we have already received via
    // previous reads -- minBytes,
    // maxBytes, and buffer have already been adjusted to account for them, but
    // this count must
    // be included in the final return value.

    ssize_t n;
    KJ_NONBLOCKING_SYSCALL(n = ::read(readFd(), buffer, maxBytes)) {
      return alreadyRead;
    }

    if (n < 0) {
      // Read would block.
      return onReadable().then([=]() {
        return tryReadInternal(buffer, minBytes, maxBytes, alreadyRead);
      });
    } else if (n == 0) {
      // EOF -OR- maxBytes == 0.
      return alreadyRead;
    } else if (kj::implicitCast<size_t>(n) < minBytes) {
      // The kernel returned fewer bytes than we asked for (and fewer than we
      // need).  This indicates
      // that we're out of data.  It could also mean we're at EOF.  We could
      // check for EOF by doing
      // another read just to see if it returns zero, but that would mean making
      // a redundant syscall
      // every time we receive a message on a long-lived connection.  So,
      // instead, we optimistically
      // asume we are not at EOF and return to the event loop.
      //
      // If libuv provided notification of HUP or RDHUP, we could do better
      // here...
      const auto bytes = static_cast<size_t>(n);
      buffer = reinterpret_cast<capnp::byte *>(buffer) + bytes;
      minBytes -= bytes;
      maxBytes -= bytes;
      alreadyRead += bytes;
      return onReadable().then([=]() {
        return tryReadInternal(buffer, minBytes, maxBytes, alreadyRead);
      });
    } else {
      // We read enough to stop here.
      return alreadyRead + static_cast<size_t>(n);
    }
  }

  kj::Promise<void> writeInternal(
      kj::ArrayPtr<const capnp::byte> firstPiece,
      kj::ArrayPtr<const kj::ArrayPtr<const capnp::byte>> morePieces) {
    KJ_STACK_ARRAY(struct iovec, iov, 1 + morePieces.size(), 16, 128);

    // writev() interface is not const-correct.  :(
    iov[0].iov_base = const_cast<capnp::byte *>(firstPiece.begin());
    iov[0].iov_len = firstPiece.size();
    for (uint i = 0; i < morePieces.size(); i++) {
      iov[i + 1].iov_base = const_cast<capnp::byte *>(morePieces[i].begin());
      iov[i + 1].iov_len = morePieces[i].size();
    }

    ssize_t writeResult;
    KJ_NONBLOCKING_SYSCALL(
        writeResult =
            ::writev(writeFd(), iov.begin(), static_cast<int>(iov.size()))) {
      // Error.

      // We can't "return kj::READY_NOW;" inside this block because it causes a
      // memory leak due to
      // a bug that exists in both Clang and GCC:
      //   http://gcc.gnu.org/bugzilla/show_bug.cgi?id=33799
      //   http://llvm.org/bugs/show_bug.cgi?id=12286
      goto error;
    }
    if (false) {
    error:
      return kj::READY_NOW;
    }

    // A negative result means EAGAIN, which we can treat the same as having
    // written zero bytes.
    size_t n = writeResult < 0 ? 0 : static_cast<size_t>(writeResult);

    // Discard all data that was written, then issue a new write for what's left
    // (if any).
    for (;;) {
      if (n < firstPiece.size()) {
        // Only part of the first piece was consumed.  Wait for POLLOUT and then
        // write again.
        firstPiece = firstPiece.slice(n, firstPiece.size());
        return onWritable().then(
            [=]() { return writeInternal(firstPiece, morePieces); });
      } else if (morePieces.size() == 0) {
        // First piece was fully-consumed and there are no more pieces, so we're
        // done.
        KJ_DASSERT(n == firstPiece.size(), n);
        return kj::READY_NOW;
      } else {
        // First piece was fully consumed, so move on to the next piece.
        n -= firstPiece.size();
        firstPiece = morePieces[0];
        morePieces = morePieces.slice(1, morePieces.size());
      }
    }
  }
};

class QtConnectionReceiver final : public kj::ConnectionReceiver,
                                   public OwnedFileDescriptor {
public:
  QtConnectionReceiver(int fd, uint flags) : OwnedFileDescriptor(fd, flags) {}

  kj::Promise<kj::Own<kj::AsyncIoStream>> accept() override {
    int newFd;

  retry:
#ifdef __linux__
    newFd = ::accept4(readFd(), nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
    newFd = ::accept(readFd(), nullptr, nullptr);
#endif

    if (newFd >= 0) {
      return kj::Own<kj::AsyncIoStream>(
          kj::heap<QtIoStream>(newFd, NEW_FD_FLAGS));
    } else {
      int error = errno;

      switch (error) {
      case EAGAIN:
#if EAGAIN != EWOULDBLOCK
      case EWOULDBLOCK:
#endif
        // Not ready yet.
        return onReadable().then([this]() { return accept(); });

      case EINTR:
      case ENETDOWN:
      case EPROTO:
      case EHOSTDOWN:
      case EHOSTUNREACH:
      case ENETUNREACH:
      case ECONNABORTED:
      case ETIMEDOUT:
        // According to the Linux man page, accept() may report an error if the
        // accepted
        // connection is already broken.  In this case, we really ought to just
        // ignore it and
        // keep waiting.  But it's hard to say exactly what errors are such
        // network errors and
        // which ones are permanent errors.  We've made a guess here.
        goto retry;

      default:
        KJ_FAIL_SYSCALL("accept", error);
      }
    }
  }

  uint getPort() override {
    socklen_t addrlen;
    union {
      struct sockaddr generic;
      struct sockaddr_in inet4;
      struct sockaddr_in6 inet6;
    } addr;
    addrlen = sizeof(addr);
    KJ_SYSCALL(getsockname(readFd(), &addr.generic, &addrlen));
    switch (addr.generic.sa_family) {
    case AF_INET:
      return ntohs(addr.inet4.sin_port);
    case AF_INET6:
      return ntohs(addr.inet6.sin6_port);
    default:
      return 0;
    }
  }
};

class QtLowLevelAsyncIoProvider final : public kj::LowLevelAsyncIoProvider {
public:
  QtLowLevelAsyncIoProvider() : waitScope(eventPort.getKjLoop()) {}

  inline kj::WaitScope &getWaitScope() { return waitScope; }

  kj::Own<kj::AsyncInputStream> wrapInputFd(int fd, uint flags = 0) override {
    return kj::heap<QtIoStream>(fd, flags);
  }
  kj::Own<kj::AsyncOutputStream> wrapOutputFd(int fd, uint flags = 0) override {
    return kj::heap<QtIoStream>(fd, flags);
  }
  kj::Own<kj::AsyncIoStream> wrapSocketFd(int fd, uint flags = 0) override {
    return kj::heap<QtIoStream>(fd, flags);
  }
  kj::Promise<kj::Own<kj::AsyncIoStream>>
  wrapConnectingSocketFd(int fd, uint flags = 0) override {
    auto result = kj::heap<QtIoStream>(fd, flags);
    auto connected = result->onWritable();
    return connected.then(
        kj::mvCapture(result, [fd](kj::Own<kj::AsyncIoStream> &&stream) {
          int err;
          socklen_t errlen = sizeof(err);
          KJ_SYSCALL(getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen));
          if (err != 0) {
            KJ_FAIL_SYSCALL("connect()", err) { break; }
          }
          return kj::mv(stream);
        }));
  }
  kj::Own<kj::ConnectionReceiver> wrapListenSocketFd(int fd,
                                                     uint flags = 0) override {
    return kj::heap<QtConnectionReceiver>(fd, flags);
  }

  kj::Timer &getTimer() override {
    // TODO(soon):  Implement this.
    KJ_FAIL_ASSERT("Timers not implemented.");
  }

private:
  QtEventPort eventPort;
  kj::WaitScope waitScope;
};

QtAsyncIoContext setupAsyncIo() {
  auto lowLevel = kj::heap<QtLowLevelAsyncIoProvider>();
  auto ioProvider = kj::newAsyncIoProvider(*lowLevel);
  auto &waitScope = lowLevel->getWaitScope();
  return {kj::mv(lowLevel), kj::mv(ioProvider), waitScope};
}
}
}

#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#include "async-io.moc"
