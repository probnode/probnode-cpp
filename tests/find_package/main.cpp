#include <probnode/client.hpp>
#include <probnode/error.hpp>
#include <probnode/live.hpp>

#include <iostream>

int main() {
  // No network: the installed headers and library link, and the bundled sha256 answers.
  std::cout << "probnode " << probnode::version() << " "
            << probnode::sha256_hex("abc").substr(0, 12) << "\n";
  probnode::ClientOptions o;
  o.from_env = false;
  probnode::Client pn(o);
  // The Live client links too: nothing listens on port 1, so it fails fast with a Transport error.
  probnode::live::Options lo;
  lo.url = "ws://127.0.0.1:1/v2/live";
  lo.reconnect = false;
  lo.connect_timeout = std::chrono::milliseconds(2000);
  bool transport = false;
  try {
    probnode::live::Live live(lo);
  } catch (const probnode::Error& e) {
    transport = e.kind() == probnode::ErrorKind::Transport;
  }
  return pn.base_url() == probnode::kDefaultBaseUrl && transport ? 0 : 1;
}
