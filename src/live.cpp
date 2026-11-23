// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
#include "probnode/live.hpp"

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <utility>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>

#include "probnode/error.hpp"

namespace probnode {
namespace live {

using clock = std::chrono::system_clock;

struct Live::Impl {
  Options opt;
  ix::WebSocket ws;
  mutable std::mutex mu;
  std::condition_variable cv;
  std::deque<Message> queue;
  std::map<std::string, std::string> subs;  // market -> its own sub message
  bool open = false;
  bool ever_open = false;
  bool done = false;  // closed for good
  std::string error;
  clock::time_point lost{};
  bool overflowed = false;
};

namespace {

// The message's "type", read without parsing the whole message: the reader parses it anyway, so
// the socket thread does not do it twice. Every message of the stream starts with its type.
std::string read_type(const std::string& s) {
  static const std::string key = "\"type\"";
  auto k = s.find(key);
  if (k == std::string::npos) return {};
  auto q = s.find('"', s.find(':', k + key.size()));
  if (q == std::string::npos) return {};
  auto e = s.find('"', q + 1);
  return e == std::string::npos ? std::string() : s.substr(q + 1, e - q - 1);
}

std::string sub_message(const std::vector<std::string>& markets, const std::vector<std::string>& channels,
                        const std::vector<std::string>& blocks) {
  nlohmann::json m = {{"op", "sub"}, {"markets", markets}, {"channels", channels}, {"format", "json"}};
  if (!blocks.empty()) m["blocks"] = blocks;
  return m.dump();
}

}  // namespace

Live::Live(Options options) : impl_(std::make_unique<Impl>()) {
  ix::initNetSystem();
  Impl& I = *impl_;
  I.opt = std::move(options);
  I.ws.setUrl(I.opt.url);
  if (!I.opt.key.empty()) I.ws.setExtraHeaders({{"Authorization", "Bearer " + I.opt.key}});
  if (I.opt.reconnect) {
    I.ws.enableAutomaticReconnection();
    I.ws.setMinWaitBetweenReconnectionRetries(500);
    I.ws.setMaxWaitBetweenReconnectionRetries(30000);
  } else {
    I.ws.disableAutomaticReconnection();
  }
  I.ws.setOnMessageCallback([&I](const ix::WebSocketMessagePtr& m) {
    std::function<void()> gap;
    {
      std::lock_guard<std::mutex> lk(I.mu);
      switch (m->type) {
        case ix::WebSocketMessageType::Open:
          if (I.ever_open) {
            // A reconnect: every market is subscribed again and starts from a fresh snapshot.
            for (const auto& s : I.subs) I.ws.sendText(s.second);
            if (I.opt.on_gap) {
              auto from = I.lost, to = clock::now();
              gap = [&I, from, to] { I.opt.on_gap(from, to); };
            }
          }
          I.open = I.ever_open = true;
          break;
        case ix::WebSocketMessageType::Close:
        case ix::WebSocketMessageType::Error:
          if (I.open) I.lost = clock::now();
          I.open = false;
          if (m->type == ix::WebSocketMessageType::Error) I.error = m->errorInfo.reason;
          if (!I.opt.reconnect) I.done = true;
          break;
        case ix::WebSocketMessageType::Message:
          if (!m->binary) {
            if (I.queue.size() >= I.opt.max_queue) {
              // The reader fell behind: start it fresh rather than hold an unbounded queue.
              I.queue.clear();
              I.overflowed = true;
            } else {
              I.queue.push_back(Message{read_type(m->str), m->str});
            }
          }
          break;
        default:
          break;
      }
    }
    if (gap) gap();
    I.cv.notify_all();
    bool restart = false;
    {
      std::lock_guard<std::mutex> lk(I.mu);
      restart = I.overflowed && !I.done;
      I.overflowed = false;
    }
    // close() from the socket's own thread; with reconnect on, IXWebSocket reopens it and the
    // Open branch resubscribes and reports the gap.
    if (restart) I.ws.close(1000, "client queue full");
  });
  I.ws.start();
  std::unique_lock<std::mutex> lk(I.mu);
  bool opened = I.cv.wait_for(lk, I.opt.connect_timeout, [&I] { return I.open || I.done; }) && I.open;
  if (!opened) {
    std::string why = I.error.empty() ? "could not open " + I.opt.url : I.error;
    lk.unlock();
    I.ws.stop();
    throw Error(ErrorKind::Transport, 0, why);
  }
}

Live::~Live() { close(); }

void Live::subscribe(const std::vector<std::string>& markets, const std::vector<std::string>& channels,
                     const std::vector<std::string>& blocks) {
  Impl& I = *impl_;
  {
    std::lock_guard<std::mutex> lk(I.mu);
    for (const auto& m : markets) I.subs[m] = sub_message({m}, channels, blocks);
  }
  I.ws.sendText(sub_message(markets, channels, blocks));
}

void Live::unsubscribe(const std::vector<std::string>& markets) {
  Impl& I = *impl_;
  {
    std::lock_guard<std::mutex> lk(I.mu);
    for (const auto& m : markets) I.subs.erase(m);
  }
  I.ws.sendText(nlohmann::json{{"op", "unsub"}, {"markets", markets}}.dump());
}

std::optional<Message> Live::next(std::chrono::milliseconds timeout) {
  Impl& I = *impl_;
  std::unique_lock<std::mutex> lk(I.mu);
  I.cv.wait_for(lk, timeout, [&I] { return !I.queue.empty() || I.done; });
  if (I.queue.empty()) return std::nullopt;
  Message m = std::move(I.queue.front());
  I.queue.pop_front();
  return m;
}

bool Live::closed() const {
  std::lock_guard<std::mutex> lk(impl_->mu);
  return impl_->done;
}

void Live::close() {
  if (!impl_) return;
  {
    std::lock_guard<std::mutex> lk(impl_->mu);
    impl_->done = true;
  }
  impl_->ws.disableAutomaticReconnection();
  impl_->ws.stop();
  impl_->cv.notify_all();
}

}  // namespace live
}  // namespace probnode
