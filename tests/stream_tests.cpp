// The Live API client against an in-process WebSocket server standing in for /v2/live.
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include "harness.hpp"
#include "probnode/error.hpp"
#include "probnode/live.hpp"

namespace {

// A stand-in for /v2/live: checks the key, answers every sub with a header, and with drop set
// closes its first connection after that answer.
struct Fake {
  ix::WebSocketServer server;
  std::atomic<int> subs{0};
  std::atomic<int> conns{0};
  std::atomic<bool> bad_key{false};
  bool drop;
  int burst = 1;  // messages sent for each sub on the first connection

  Fake(int port, bool drop_first, int burst_first = 1)
      : server((ix::initNetSystem(), port), "127.0.0.1"), drop(drop_first), burst(burst_first) {
    // initNetSystem: WSAStartup on Windows, before the server's first socket call.
    server.setOnClientMessageCallback([this](std::shared_ptr<ix::ConnectionState>, ix::WebSocket& ws,
                                             const ix::WebSocketMessagePtr& m) {
      if (m->type == ix::WebSocketMessageType::Open) {
        conns++;
        auto it = m->openInfo.headers.find("Authorization");
        if (it == m->openInfo.headers.end() || it->second != "Bearer k") bad_key = true;
      } else if (m->type == ix::WebSocketMessageType::Message && m->str.find("\"op\":\"sub\"") != std::string::npos) {
        subs++;
        int n = conns == 1 ? burst : 1;
        for (int i = 0; i < n; i++) ws.sendText(R"({"type":"header","market":"m1"})");
        if (drop && conns == 1) ws.close();
      }
    });
    auto res = server.listen();
    if (!res.first) throw std::runtime_error("listen: " + res.second);
    server.start();
  }
  ~Fake() { server.stop(); }
};

std::string url(int port) { return "ws://127.0.0.1:" + std::to_string(port) + "/v2/live"; }

}  // namespace

TEST(live_subscribe_and_read) {
  Fake f(18761, false);
  probnode::live::Options o;
  o.url = url(18761);
  o.key = "k";
  probnode::live::Live live(o);
  live.subscribe({"m1"}, {"book+"});
  auto m = live.next(std::chrono::seconds(5));
  CHECK(m.has_value());
  CHECK_EQ(m->type, std::string("header"));
  CHECK(m->json.find("\"market\":\"m1\"") != std::string::npos);
  CHECK_EQ(f.subs.load(), 1);
  CHECK(!f.bad_key.load());
}

TEST(live_reconnect_resubscribes_and_reports_the_gap) {
  Fake f(18762, true);
  std::atomic<int> gaps{0};
  probnode::live::Options o;
  o.url = url(18762);
  o.key = "k";
  o.on_gap = [&gaps](auto from, auto to) {
    if (to >= from) gaps++;
  };
  probnode::live::Live live(o);
  live.subscribe({"m1"}, {"book+"});
  CHECK(live.next(std::chrono::seconds(5)).has_value());   // the first header
  CHECK(live.next(std::chrono::seconds(10)).has_value());  // the header after the reconnect
  CHECK_EQ(f.subs.load(), 2);
  CHECK_EQ(gaps.load(), 1);
}

TEST(live_unreachable_is_a_transport_error) {
  probnode::live::Options o;
  o.url = url(18763);  // nothing listens
  o.reconnect = false;
  o.connect_timeout = std::chrono::seconds(3);
  bool threw = false;
  try {
    probnode::live::Live live(o);
  } catch (const probnode::Error& e) {
    threw = e.kind() == probnode::ErrorKind::Transport;
  }
  CHECK(threw);
}

TEST(live_close_ends_next) {
  Fake f(18764, false);
  probnode::live::Options o;
  o.url = url(18764);
  o.key = "k";
  probnode::live::Live live(o);
  live.close();
  CHECK(live.closed());
  CHECK(!live.next(std::chrono::milliseconds(200)).has_value());
}

TEST(live_a_full_queue_restarts_the_reader) {
  Fake f(18765, false, 50);
  std::atomic<int> gaps{0};
  probnode::live::Options o;
  o.url = url(18765);
  o.key = "k";
  o.max_queue = 5;
  o.on_gap = [&gaps](auto, auto) { gaps++; };
  probnode::live::Live live(o);
  live.subscribe({"m1"}, {"book+"});
  // 50 messages into a queue of 5: the client drops the backlog, reconnects and resubscribes.
  for (int i = 0; i < 100 && f.subs.load() < 2; i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  CHECK_EQ(f.subs.load(), 2);
  CHECK(live.next(std::chrono::seconds(5)).has_value());
  CHECK_EQ(gaps.load(), 1);
}

int main(int argc, char** argv) { return harness::run(argc, argv); }
