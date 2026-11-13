// The client against a mock API (cpp-httplib, in-process): requests as the API expects them,
// answers parsed, errors carried, downloads checked.
#include <httplib.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <random>
#include <thread>

#include "harness.hpp"
#include "probnode/client.hpp"

namespace fs = std::filesystem;
using nlohmann::json;
using probnode::ErrorKind;

namespace {

struct Seen {
  std::string method;
  std::string path;
  std::multimap<std::string, std::string> params;
  std::string authorization;
  std::string body;
};

/// A mock API on a free localhost port, recording every request.
class Mock {
 public:
  Mock() {
    // After routing (the body has been read), before the response is written.
    server.set_post_routing_handler([this](const httplib::Request& req, httplib::Response&) {
      std::lock_guard<std::mutex> lock(mutex);
      seen.push_back(Seen{req.method, req.path,
                          std::multimap<std::string, std::string>(req.params.begin(),
                                                                  req.params.end()),
                          req.get_header_value("Authorization"), req.body});
    });
    port = server.bind_to_any_port("127.0.0.1");
    thread = std::thread([this] { server.listen_after_bind(); });
    server.wait_until_ready();
    base = "http://127.0.0.1:" + std::to_string(port);
  }
  ~Mock() {
    server.stop();
    thread.join();
  }

  void json_route(const std::string& method, const std::string& path, json body, int status = 200) {
    auto handler = [body, status](const httplib::Request&, httplib::Response& res) {
      res.status = status;
      res.set_content(body.dump(), "application/json");
    };
    if (method == "GET") server.Get(path, handler);
    else server.Post(path, handler);
  }

  void bytes_route(const std::string& path, std::string body, int status = 200) {
    server.Get(path, [body, status](const httplib::Request&, httplib::Response& res) {
      res.status = status;
      res.set_content(body, "application/octet-stream");
    });
  }

  std::vector<Seen> requests() {
    std::lock_guard<std::mutex> lock(mutex);
    return seen;
  }

  probnode::Client keyed() const { return client("pn_test"); }
  probnode::Client keyless() const { return client(""); }

  probnode::Client client(const std::string& key) const {
    probnode::ClientOptions o;
    if (!key.empty()) o.api_key = key;
    o.base_url = base + "/";
    o.from_env = false;
    return probnode::Client(o);
  }

  httplib::Server server;
  std::string base;

 private:
  int port = 0;
  std::thread thread;
  std::mutex mutex;
  std::vector<Seen> seen;
};

/// A fresh temporary directory, removed afterwards.
struct TempDir {
  fs::path path;
  TempDir() {
    std::random_device rd;
    path = fs::temp_directory_path() / ("probnode-test-" + std::to_string(rd()) + std::to_string(rd()));
    fs::create_directories(path);
  }
  ~TempDir() {
    std::error_code ec;
    fs::remove_all(path, ec);
  }
};

std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

json file_json(const std::string& object_path, const std::string& body) {
  return {{"object_path", object_path}, {"dataset", "trades"}, {"format", "parquet"},
          {"event_date", "2026-11-12"}, {"hour", 10}, {"src", "all"}, {"bytes", body.size()},
          {"rows", 3}, {"sha256", probnode::sha256_hex(body)}, {"granularity", "hour"},
          {"market", nullptr}, {"era", "v4"}, {"new_field", 7}};
}

probnode::DataFile data_file(const std::string& object_path, const std::string& body) {
  probnode::DataFile f;
  f.object_path = object_path;
  f.dataset = "trades";
  f.format = "parquet";
  f.bytes = body.size();
  f.sha256 = probnode::sha256_hex(body);
  return f;
}

json signed_json(const std::string& base, const std::vector<std::string>& paths) {
  json urls = json::array();
  for (const auto& p : paths) {
    urls.push_back({{"object_path", p}, {"url", base + "/store/" + p}, {"expires_at", "x"}});
  }
  return {{"urls", urls}, {"licence", "l"}, {"licence_detail", "d"}};
}

}  // namespace

TEST(sha256_vectors) {
  CHECK_EQ(probnode::sha256_hex(""),
           std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  CHECK_EQ(probnode::sha256_hex("abc"),
           std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  CHECK_EQ(probnode::sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
           std::string("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
  CHECK_EQ(probnode::sha256_hex(std::string(1000000, 'a')),
           std::string("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

TEST(sha256_streaming) {
  std::string data;
  for (int i = 0; i < 1000; ++i) data += std::to_string(i * 7919) + ",";
  probnode::Sha256 h;
  for (std::size_t i = 0; i < data.size(); i += 37) h.update(data.substr(i, 37));
  CHECK_EQ(h.hex(), probnode::sha256_hex(data));
  TempDir dir;
  std::ofstream(dir.path / "f", std::ios::binary) << data;
  CHECK_EQ(probnode::sha256_file(dir.path / "f"), probnode::sha256_hex(data));
}

TEST(datasets_are_public_and_typed) {
  Mock mock;
  mock.json_route("GET", "/v1/data/datasets",
                  json::array({{{"code", "book_full"}, {"title", "Full order book"},
                                {"formats", {"evmin"}}, {"market_day_formats", {"parquet", "csv"}},
                                {"tier", "tape"}, {"first_hour", "2026-11-12T06:00:00Z"},
                                {"last_hour", nullptr}, {"schema", "book-max"},
                                {"aliases", {"book_full"}}, {"legacy", false},
                                {"something_new", true}}}));
  auto ds = mock.keyless().datasets();
  CHECK_EQ(ds.size(), 1u);
  CHECK_EQ(ds[0].code, std::string("book_full"));
  CHECK_EQ(ds[0].market_day_formats.size(), 2u);
  CHECK_EQ(ds[0].market_day_formats[1], std::string("csv"));
  CHECK(ds[0].first_hour.has_value());
  CHECK(!ds[0].last_hour.has_value());
  CHECK_EQ(*ds[0].schema, std::string("book-max"));
  CHECK(mock.requests().at(0).authorization.empty());
}

TEST(the_key_is_sent_as_bearer) {
  Mock mock;
  mock.json_route("GET", "/v1/data/usage",
                  {{"plan", "pro"}, {"datasets", {"trades"}}, {"signed_urls_used_today", 3},
                   {"signed_urls_per_day", 1000}, {"requests_per_minute", 120},
                   {"resets_at", "2026-11-13T00:00:00Z"}, {"freshness", "realtime"},
                   {"freshness_cutoff", nullptr}});
  auto usage = mock.keyed().usage();
  CHECK_EQ(usage.plan, std::string("pro"));
  CHECK_EQ(usage.signed_urls_per_day, 1000u);
  CHECK_EQ(mock.requests().at(0).authorization, std::string("Bearer pn_test"));
}

TEST(a_keyed_call_without_a_key_sends_nothing) {
  Mock mock;
  auto err = CHECK_THROWS_AS(mock.keyless().me(), ErrorKind::NoApiKey);
  CHECK_EQ(err.status(), 401);
  CHECK(mock.requests().empty());
}

TEST(errors_carry_status_and_the_api_sentence) {
  Mock mock;
  mock.json_route("POST", "/v1/data/exports",
                  {{"detail", "your plan does not include book-max"}}, 402);
  mock.json_route("GET", "/v1/catalog/coverage",
                  {{"detail", json::array({{{"type", "missing"},
                                            {"loc", {"query", "token_id"}},
                                            {"msg", "Field required"}}})}},
                  422);
  mock.server.Get("/v1/data/datasets", [](const httplib::Request&, httplib::Response& res) {
    res.status = 503;
    res.set_content("<html>down</html>", "text/html");
  });
  auto pn = mock.keyed();

  auto err = CHECK_THROWS_AS(pn.export_book("m", "2026-11-12"), ErrorKind::Api);
  CHECK_EQ(err.status(), 402);
  CHECK_EQ(err.detail(), std::string("your plan does not include book-max"));
  CHECK_EQ(std::string(err.what()), std::string("402: your plan does not include book-max"));

  err = CHECK_THROWS_AS(pn.coverage("x"), ErrorKind::Api);
  CHECK_EQ(err.status(), 422);
  CHECK(err.detail().find("Field required") != std::string::npos);

  err = CHECK_THROWS_AS(pn.datasets(), ErrorKind::Api);
  CHECK_EQ(std::string(err.what()), std::string("503: Service Unavailable"));
}

TEST(catalog_queries) {
  Mock mock;
  mock.json_route("GET", "/v1/catalog/series",
                  json::array({{{"slug", "bitcoin-above"}, {"title", "Bitcoin above"},
                                {"venue", "polymarket"}, {"category", "crypto"}}}));
  mock.json_route("GET", "/v1/catalog/markets",
                  json::array({{{"slug", "btc-1"}, {"series_slug", "bitcoin-above"},
                                {"start_ts", "2026-11-12T00:00:00Z"},
                                {"end_ts", "2026-11-13T00:00:00Z"}, {"resolution", nullptr},
                                {"tokens", json::array({{{"token_id", "1"}, {"outcome", "Yes"}},
                                                        {{"token_id", "2"}, {"outcome", "No"}}})}}}));
  mock.json_route("GET", "/v1/catalog/coverage",
                  json::array({{{"token_id", "1"}, {"hour_start", "2026-11-12T00:00:00Z"},
                                {"recorded_ms", 3600000}, {"gap_count", 0}, {"uptime_pct", 100.0},
                                {"implied", false}, {"derived_ms", 0}, {"era", "v4"},
                                {"era_split_ms", nullptr}, {"clock_stratum", 2}}}));
  mock.json_route("GET", "/v1/samples",
                  json::array({{{"url", "https://example.invalid/s.parquet"},
                                {"object_path", "s.parquet"}, {"dataset", "trades"},
                                {"format", "parquet"}, {"event_date", "2026-11-12"}, {"hour", 0},
                                {"src", "all"}, {"bytes", 1}, {"rows", 1}, {"sha256", "00"},
                                {"granularity", "market_day"}, {"market", "btc-1"}, {"era", "v4"}}}));
  auto pn = mock.keyless();
  CHECK_EQ(pn.series("crypto", "bitcoin").at(0).slug, std::string("bitcoin-above"));
  probnode::MarketQuery q;
  q.series = "bitcoin-above";
  q.limit = 2;
  auto markets = pn.markets(q);
  CHECK_EQ(markets.at(0).tokens.at(1).outcome, std::string("No"));
  CHECK(!markets.at(0).resolution.has_value());
  auto cov = pn.coverage("1", "2026-11-12T00:00:00Z");
  CHECK_EQ(cov.at(0).recorded_ms, 3600000);
  CHECK(cov.at(0).clock_stratum.has_value() && *cov.at(0).clock_stratum == 2);
  CHECK(!cov.at(0).era_split_ms.has_value());
  auto samples = pn.samples();
  CHECK_EQ(*samples.at(0).file.market, std::string("btc-1"));
  CHECK_EQ(samples.at(0).url, std::string("https://example.invalid/s.parquet"));

  auto seen = mock.requests();
  CHECK_EQ(seen.at(0).params.find("category")->second, std::string("crypto"));
  CHECK_EQ(seen.at(0).params.find("q")->second, std::string("bitcoin"));
  CHECK_EQ(seen.at(1).params.find("series")->second, std::string("bitcoin-above"));
  CHECK_EQ(seen.at(1).params.find("limit")->second, std::string("2"));
  CHECK_EQ(seen.at(2).params.find("start")->second, std::string("2026-11-12T00:00:00Z"));
  CHECK(seen.at(2).params.find("end") == seen.at(2).params.end());
}

TEST(files_send_every_filter) {
  Mock mock;
  mock.json_route("GET", "/v1/data/files",
                  {{"files", json::array({file_json("trades/a.csv.gz", "x")})},
                   {"total_files", 1}, {"total_bytes", 1}});
  probnode::FilesQuery fq;
  fq.granularity = "market_day";
  fq.markets = {"m1", "m2"};
  fq.series = {"s1"};
  auto files = mock.keyed().files("trades", "csv", "2026-11-12T00:00:00Z", "2026-11-13T00:00:00Z", fq);
  CHECK_EQ(files.size(), 1u);
  CHECK_EQ(files[0].hour, 10);
  CHECK_EQ(files[0].sha256, probnode::sha256_hex("x"));
  auto p = mock.requests().at(0).params;
  CHECK_EQ(p.count("market"), 2u);
  auto range = p.equal_range("market");
  CHECK_EQ(range.first->second, std::string("m1"));
  CHECK_EQ(std::next(range.first)->second, std::string("m2"));
  CHECK_EQ(p.find("series")->second, std::string("s1"));
  CHECK_EQ(p.find("granularity")->second, std::string("market_day"));
  CHECK_EQ(p.find("start")->second, std::string("2026-11-12T00:00:00Z"));
  CHECK_EQ(p.find("format")->second, std::string("csv"));
}

TEST(sign_batches_by_one_hundred) {
  Mock mock;
  std::atomic<int> calls{0};
  mock.server.Post("/v1/data/urls", [&](const httplib::Request& req, httplib::Response& res) {
    ++calls;
    auto body = json::parse(req.body);
    std::vector<std::string> paths = body["paths"].get<std::vector<std::string>>();
    if (paths.size() > 100) res.status = 422;
    res.set_content(signed_json("https://x.invalid", paths).dump(), "application/json");
  });
  std::vector<std::string> paths;
  for (int i = 0; i < 250; ++i) paths.push_back("p/" + std::to_string(i));
  auto urls = mock.keyed().sign(paths);
  CHECK_EQ(calls.load(), 3);
  CHECK_EQ(urls.size(), 250u);
  CHECK_EQ(urls["p/249"], std::string("https://x.invalid/store/p/249"));
}

TEST(download_checks_sha256_and_renames) {
  Mock mock;
  std::string body = "PAR1 some parquet bytes PAR1";
  std::string op = "trades/parquet/2026-11-12/10.parquet";
  mock.json_route("POST", "/v1/data/urls", signed_json(mock.base, {op}));
  mock.bytes_route("/store/" + op, body);
  TempDir dir;
  auto got = mock.keyed().download(data_file(op, body), dir.path);
  CHECK(got == dir.path / "trades" / "parquet" / "2026-11-12" / "10.parquet");
  CHECK_EQ(read_file(got), body);
  CHECK(!fs::exists(dir.path / "trades/parquet/2026-11-12/10.parquet.part"));
  // The signed link is fetched without the API key.
  for (const auto& r : mock.requests()) {
    if (r.path.rfind("/store/", 0) == 0) CHECK(r.authorization.empty());
    else CHECK_EQ(r.authorization, std::string("Bearer pn_test"));
  }
}

TEST(a_sha256_mismatch_leaves_nothing) {
  Mock mock;
  mock.json_route("POST", "/v1/data/urls", signed_json(mock.base, {"t/bad.parquet"}));
  mock.bytes_route("/store/t/bad.parquet", "tampered");
  TempDir dir;
  auto file = data_file("t/bad.parquet", "original");
  auto err = CHECK_THROWS_AS(mock.keyed().download(file, dir.path), ErrorKind::Checksum);
  CHECK(err.detail().find(probnode::sha256_hex("original")) != std::string::npos);
  CHECK(err.detail().find(probnode::sha256_hex("tampered")) != std::string::npos);
  CHECK(!fs::exists(dir.path / "t" / "bad.parquet"));
  CHECK(!fs::exists(dir.path / "t" / "bad.parquet.part"));
  // verify = false keeps it.
  probnode::DownloadOptions o;
  o.verify = false;
  auto got = mock.keyed().download(file, dir.path, o);
  CHECK_EQ(read_file(got), std::string("tampered"));
}

TEST(a_failed_fetch_is_an_error_with_its_status) {
  Mock mock;
  mock.bytes_route("/store/gone", "denied", 403);
  TempDir dir;
  probnode::DownloadOptions o;
  o.url = mock.base + "/store/gone";
  auto err = CHECK_THROWS_AS(mock.keyed().download(data_file("gone", "x"), dir.path, o),
                             ErrorKind::Api);
  CHECK_EQ(err.status(), 403);
  CHECK(!fs::exists(dir.path / "gone"));
  CHECK(!fs::exists(dir.path / "gone.part"));
}

TEST(download_all_signs_once_for_many) {
  Mock mock;
  std::atomic<int> calls{0};
  mock.server.Post("/v1/data/urls", [&](const httplib::Request& req, httplib::Response& res) {
    ++calls;
    auto paths = json::parse(req.body)["paths"].get<std::vector<std::string>>();
    res.set_content(signed_json(mock.base, paths).dump(), "application/json");
  });
  mock.bytes_route("/store/a/1", "one");
  mock.bytes_route("/store/a/2", "two");
  TempDir dir;
  auto got = mock.keyed().download_all({data_file("a/1", "one"), data_file("a/2", "two")}, dir.path);
  CHECK_EQ(calls.load(), 1);
  CHECK_EQ(got.size(), 2u);
  CHECK_EQ(read_file(got[1]), std::string("two"));
}

TEST(export_book_sends_schema_and_format) {
  Mock mock;
  std::string body = "csv,gz,bytes";
  std::string op = "trades/market_day/2026-11-12/will-x-happen.csv.gz";
  mock.json_route("POST", "/v1/data/exports",
                  {{"urls", signed_json(mock.base, {op})["urls"]}, {"licence", "l"},
                   {"licence_detail", "d"}, {"schema", "trades"}, {"rows", 1234},
                   {"bytes", body.size()}, {"sha256", probnode::sha256_hex(body)}, {"built", true}});
  mock.bytes_route("/store/" + op, body);
  auto pn = mock.keyed();
  probnode::ExportOptions o;
  o.schema = "trades";
  o.format = "csv";
  auto day = pn.export_book("will-x-happen", "2026-11-12", o);
  CHECK_EQ(day.rows, 1234u);
  CHECK(day.built);
  CHECK_EQ(day.object_path(), op);
  auto sent = json::parse(mock.requests().at(0).body);
  CHECK_EQ(sent["market"].get<std::string>(), std::string("will-x-happen"));
  CHECK_EQ(sent["day"].get<std::string>(), std::string("2026-11-12"));
  CHECK_EQ(sent["schema"].get<std::string>(), std::string("trades"));
  CHECK_EQ(sent["format"].get<std::string>(), std::string("csv"));
  TempDir dir;
  auto path = pn.download(day, dir.path);
  CHECK(path == dir.path / fs::u8path(op));
  CHECK_EQ(read_file(path), body);
}

TEST(export_book_defaults_to_book_max_parquet) {
  Mock mock;
  mock.json_route("POST", "/v1/data/exports",
                  {{"urls", signed_json("https://x.invalid", {"x"})["urls"]}, {"licence", "l"},
                   {"licence_detail", "d"}, {"schema", "book-max"}, {"rows", 1}, {"bytes", 1},
                   {"sha256", "00"}, {"built", false}});
  auto day = mock.keyed().export_book("m", "2026-11-12");
  CHECK_EQ(day.schema, std::string("book-max"));
  auto sent = json::parse(mock.requests().at(0).body);
  CHECK_EQ(sent["schema"].get<std::string>(), std::string("book-max"));
  CHECK_EQ(sent["format"].get<std::string>(), std::string("parquet"));
}

TEST(login_mints_a_key_and_uses_it) {
  Mock mock;
  mock.json_route("POST", "/v1/auth/login",
                  {{"key", "pn_new"}, {"prefix", "pn_new"}, {"name", "cli"}});
  mock.json_route("GET", "/v1/me",
                  {{"id", "6c1f"}, {"email", "a@example.com"}, {"plan", "free"},
                   {"plan_expires_at", nullptr}, {"is_admin", false}, {"email_verified", true},
                   {"created_at", "2026-11-01T00:00:00Z"}});
  auto pn = mock.keyless();
  CHECK_EQ(pn.login("a@example.com", "correct-horse"), std::string("pn_new"));
  CHECK_EQ(pn.me().plan, std::string("free"));
  auto seen = mock.requests();
  auto sent = json::parse(seen.at(0).body);
  CHECK_EQ(sent["email"].get<std::string>(), std::string("a@example.com"));
  CHECK_EQ(seen.at(1).authorization, std::string("Bearer pn_new"));
}

TEST(timeseries_parquet_and_ndjson) {
  Mock mock;
  mock.server.Get("/v1/timeseries", [](const httplib::Request& req, httplib::Response& res) {
    if (req.get_param_value("format") == "ndjson") {
      res.set_content("{\"ts\":1,\"px\":0.5}\n{\"ts\":2,\"px\":0.51}\n", "application/x-ndjson");
    } else {
      res.set_content("PAR1", "application/vnd.apache.parquet");
    }
  });
  auto pn = mock.keyed();
  auto rows = pn.timeseries_ndjson("trades", "m", "2026-11-12T00:00:00Z", "2026-11-12T01:00:00Z");
  CHECK_EQ(rows.size(), 2u);
  CHECK_EQ(json::parse(rows[1])["px"].get<double>(), 0.51);
  CHECK_EQ(pn.timeseries("trades", "m", "2026-11-12T00:00:00Z", "2026-11-12T01:00:00Z"),
           std::string("PAR1"));
  CHECK_EQ(mock.requests().at(0).params.find("market")->second, std::string("m"));
}

TEST(redirects_are_not_followed_with_the_key) {
  Mock mock;
  mock.server.Get("/v1/me", [&](const httplib::Request&, httplib::Response& res) {
    res.status = 307;
    res.set_header("Location", mock.base + "/elsewhere");
  });
  mock.json_route("GET", "/elsewhere", {{"id", "x"}});
  CHECK_THROWS_AS(mock.keyed().me(), ErrorKind::Json);
  for (const auto& r : mock.requests()) CHECK(r.path != "/elsewhere");
}

TEST(object_paths_stay_inside_dest) {
  Mock mock;
  TempDir dir;
  probnode::DownloadOptions o;
  o.url = mock.base + "/never";
  for (const char* bad : {"", "../x", "a/../../x", "/etc/passwd", "a\\..\\x", "C:/x"}) {
    CHECK_THROWS_AS(mock.keyed().download(data_file(bad, "x"), dir.path, o), ErrorKind::Invalid);
  }
  CHECK(mock.requests().empty());
}

TEST(config_round_trips_with_the_python_cli_file) {
  TempDir dir;
  fs::path p = dir.path / "sub" / "probnode.json";
  auto empty = probnode::Config::load_from(p);
  CHECK(!empty.api_key && !empty.base_url);
  fs::create_directories(p.parent_path());
  // What `probnode login` (Python) writes, plus a key of its own.
  std::ofstream(p, std::ios::binary)
      << "{\n  \"api_key\": \"pn_saved\",\n  \"base_url\": \"https://probnode.com\",\n"
         "  \"other\": 1\n}";
  auto c = probnode::Config::load_from(p);
  CHECK_EQ(*c.api_key, std::string("pn_saved"));
  CHECK_EQ(*c.base_url, std::string("https://probnode.com"));
  c.api_key = "pn_new";
  c.save_to(p);
  auto again = json::parse(read_file(p));
  CHECK_EQ(again["api_key"].get<std::string>(), std::string("pn_new"));
  CHECK_EQ(again["other"].get<int>(), 1);
}

TEST(every_market_day_schema_is_listed) {
  CHECK_EQ(probnode::kMarketDaySchemas.size(), 15u);
  CHECK(probnode::kMarketDaySchemas.front() == "book-max");
  CHECK(probnode::kMarketDaySchemas.back() == "market-events");
  CHECK_EQ(std::string(probnode::version()), std::string("0.1.0"));
}

int main(int argc, char** argv) { return harness::run(argc, argv); }
