// The datasets, their formats and the hours held, and a few markets of a series. Needs no key.
//
//   probnode_datasets                  # every dataset, then five markets
//   probnode_datasets bitcoin-above    # five markets of one series
#include <probnode/client.hpp>

#include <iomanip>
#include <iostream>

int main(int argc, char** argv) {
  try {
    probnode::Client pn;
    for (const auto& d : pn.datasets()) {
      std::string formats;
      for (const auto* list : {&d.formats, &d.market_day_formats}) {
        for (const auto& f : *list) formats += (formats.empty() ? "" : ",") + f;
      }
      std::string held = d.first_hour && d.last_hour ? *d.first_hour + " .. " + *d.last_hour
                                                      : "nothing held";
      std::cout << std::left << std::setw(15) << d.code << std::setw(6) << d.tier
                << std::setw(23) << formats << held << "\n";
    }

    probnode::MarketQuery query;
    if (argc > 1) query.series = argv[1];
    query.limit = 5;
    std::cout << "\n";
    for (const auto& m : pn.markets(query)) {
      std::cout << m.slug << "  " << m.start_ts << " .. " << m.end_ts << " ";
      for (const auto& t : m.tokens) std::cout << " [" << t.outcome << "]";
      std::cout << "\n";
    }
  } catch (const probnode::Error& e) {
    std::cerr << "probnode: " << e.what() << "\n";
    return 2;
  }
  return 0;
}
