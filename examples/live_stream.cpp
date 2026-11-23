// Streams one market from the Live API and prints each message's type.
//   PROBNODE_API_KEY=... probnode_live_stream btc-updown-15m-1795366800
#include <cstdlib>
#include <iostream>

#include "probnode/live.hpp"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: probnode_live_stream <market-slug>\n";
    return 2;
  }
  probnode::live::Options opt;
  if (const char* k = std::getenv("PROBNODE_API_KEY")) opt.key = k;
  probnode::live::Live live(opt);
  live.subscribe({argv[1]}, {"book+", "trades+"});
  while (auto m = live.next()) std::cout << m->type << ' ' << m->json.substr(0, 120) << '\n';
  return 0;
}
