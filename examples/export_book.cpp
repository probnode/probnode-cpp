// One market's UTC day of a schema, made on demand, downloaded and sha256-checked.
//
//   export PROBNODE_API_KEY=pn_...     # or run `probnode login` once
//   probnode_export_book will-x-happen 2026-11-12
//   probnode_export_book will-x-happen 2026-11-12 trades csv
#include <probnode/client.hpp>

#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: probnode_export_book <market-slug> <YYYY-MM-DD> [schema] [parquet|csv]\n";
    return 2;
  }
  std::string market = argv[1];
  std::string date = argv[2];
  std::string schema = argc > 3 ? argv[3] : "book-max";
  std::string format = argc > 4 ? argv[4] : "parquet";
  try {
    probnode::Client pn;  // from env / saved key
    auto day = pn.export_book(market, date, {.schema = schema, .format = format});
    auto path = pn.download(day, "data/");  // sha256-checked; std::filesystem::path
    std::cout << market << " " << day.schema << ": " << day.rows << " rows, " << day.bytes
              << " bytes" << (day.built ? " (built now)" : "") << " -> " << path.string() << "\n";
  } catch (const probnode::Error& e) {
    std::cerr << "probnode: " << e.what() << "\n";
    return 2;
  }
  return 0;
}
