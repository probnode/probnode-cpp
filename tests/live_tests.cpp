// The client against a running API, keyless endpoints only. Each case is skipped (exit 77)
// unless PROBNODE_LIVE_BASE_URL is set, for example to https://probnode.com.
#include <cstdlib>
#include <filesystem>
#include <optional>

#include "harness.hpp"
#include "probnode/client.hpp"

namespace fs = std::filesystem;
using probnode::ErrorKind;

namespace {

probnode::Client live(const char* key = nullptr) {
  const char* url = std::getenv("PROBNODE_LIVE_BASE_URL");  // NOLINT: test-only
  if (url == nullptr || *url == '\0') throw harness::Skip();
  probnode::ClientOptions o;
  o.base_url = url;
  if (key != nullptr) o.api_key = key;
  o.from_env = false;  // no key on purpose: these are the public endpoints
  return probnode::Client(o);
}

}  // namespace

TEST(live_datasets) {
  auto ds = live().datasets();
  CHECK(!ds.empty());
  bool found = false;
  for (const auto& d : ds) {
    if (d.code != "book_full") continue;
    found = true;
    bool evmin = false, parquet = false;
    for (const auto& f : d.formats) evmin = evmin || f == "evmin";
    for (const auto& f : d.market_day_formats) parquet = parquet || f == "parquet";
    CHECK(evmin);
    CHECK(parquet);
  }
  CHECK(found);
}

TEST(live_catalog) {
  auto pn = live();
  for (const auto& s : pn.series("", "bitcoin")) CHECK(!s.slug.empty());
  probnode::MarketQuery q;
  q.limit = 3;
  auto markets = pn.markets(q);
  CHECK(!markets.empty() && markets.size() <= 3);
  std::optional<std::string> token;
  for (const auto& m : markets) {
    if (!m.tokens.empty()) {
      token = m.tokens.front().token_id;
      break;
    }
  }
  CHECK(token.has_value());
  for (const auto& h : pn.coverage(*token)) CHECK_EQ(h.token_id, *token);
}

TEST(live_samples) {
  auto pn = live();
  auto samples = pn.samples();
  const probnode::Sample* smallest = nullptr;
  for (const auto& s : samples) {
    if (!smallest || s.file.bytes < smallest->file.bytes) smallest = &s;
  }
  if (smallest && smallest->file.bytes < 20000000) {
    fs::path dir = fs::temp_directory_path() / "probnode-live-sample";
    auto path = pn.download(*smallest, dir);
    CHECK_EQ(fs::file_size(path), smallest->file.bytes);
    std::error_code ec;
    fs::remove_all(dir, ec);
  }
}

TEST(live_files_need_a_key) {
  auto pn = live();
  auto err = CHECK_THROWS_AS(
      pn.files("trades", "parquet", "2026-11-12T00:00:00Z", "2026-11-12T01:00:00Z"),
      ErrorKind::NoApiKey);
  CHECK_EQ(err.status(), 401);
  auto bad = live("pn_not_a_key");
  err = CHECK_THROWS_AS(
      bad.files("trades", "parquet", "2026-11-12T00:00:00Z", "2026-11-12T01:00:00Z"),
      ErrorKind::Api);
  CHECK_EQ(err.status(), 401);
}

int main(int argc, char** argv) { return harness::run(argc, argv); }
