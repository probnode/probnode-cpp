// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
#pragma once

// The probnode Live API from C++: every Polymarket book update and trade of the markets you
// subscribe to, as it happens, over one WebSocket - each enriched with the probnode object
// (analytics, risk against the normal for similar markets, security, stats).
//
//   probnode::live::Options opt;
//   opt.key = std::getenv("PROBNODE_API_KEY");          // empty: the Free plan
//   probnode::live::Live live(opt);
//   live.subscribe({"btc-updown-15m-1795366800"}, {"book+", "trades+"});
//   while (auto m = live.next()) std::cout << m->type << '\n';
//
// Messages are JSON objects, handed over as text with their "type" read out, so any JSON
// library parses them; this header adds no dependency of its own. This release reads JSON;
// Probe, the binary format, comes in 0.3.0. With reconnect on (the
// default), a dropped connection is reopened with backoff (0.5 s to 30 s), every subscription is
// sent again and each market starts from a fresh snapshot; on_gap reports the stretch missed.

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace probnode {
namespace live {

/// The stream's address.
inline constexpr const char* kUrl = "wss://live.probnode.com/v2/live";

/// How to connect.
struct Options {
  std::string url = kUrl;
  /// The account's API key, sent as "Authorization: Bearer <key>"; empty is the Free plan.
  std::string key;
  /// Reopen a dropped connection and subscribe again.
  bool reconnect = true;
  /// After a reconnect: the wall-clock stretch nothing was received.
  std::function<void(std::chrono::system_clock::time_point from, std::chrono::system_clock::time_point to)>
      on_gap;
  /// How long the constructor waits for the connection to open.
  std::chrono::milliseconds connect_timeout{15000};
  /// Messages held for next() at most. A reader that falls this far behind gets a fresh start:
  /// the queue is cleared, the connection reopened and every market resubscribed (fresh
  /// snapshots), and on_gap reports the stretch - never an unbounded queue.
  std::size_t max_queue = 100000;
};

/// One message of the stream.
struct Message {
  /// Its "type": "header", "norms", "symbol", "book+", "trade+", "trade_ext", "signal", ...
  std::string type;
  /// The message itself, a JSON object.
  std::string json;
};

/// One connection. Not copyable; safe to call from one thread at a time.
class Live {
 public:
  /// Connects; throws probnode::Error (ErrorKind::Transport) when it cannot.
  explicit Live(Options options = {});
  ~Live();
  Live(const Live&) = delete;
  Live& operator=(const Live&) = delete;

  /// Watches markets on channels ("book+", "trades+", "book", "trades", "fills", ...).
  /// blocks picks the probnode blocks ("analytics", "risk", "security", "stats"); empty is all.
  void subscribe(const std::vector<std::string>& markets, const std::vector<std::string>& channels,
                 const std::vector<std::string>& blocks = {});
  /// Stops watching markets, every channel of them.
  void unsubscribe(const std::vector<std::string>& markets);
  /// The next message, or nothing if none came within timeout or the connection is closed for
  /// good (closed() then says so).
  std::optional<Message> next(std::chrono::milliseconds timeout = std::chrono::seconds(30));
  /// True once the connection is closed and will not reopen.
  bool closed() const;
  /// Closes the connection; next() returns nothing from then on.
  void close();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace live
}  // namespace probnode
