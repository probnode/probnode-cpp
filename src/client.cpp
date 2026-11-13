// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
#include "probnode/client.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#include "http.hpp"
#include "parse.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

namespace probnode {
namespace {

using Query = std::vector<std::pair<std::string, std::string>>;

std::optional<std::string> env(const char* name) {
#ifdef _MSC_VER
  char* value = nullptr;
  std::size_t len = 0;
  if (_dupenv_s(&value, &len, name) != 0 || value == nullptr) return std::nullopt;
  std::string out(value);
  std::free(value);
#else
  const char* value = std::getenv(name);
  if (value == nullptr) return std::nullopt;
  std::string out(value);
#endif
  if (out.empty()) return std::nullopt;
  return out;
}

void push(Query& q, const char* key, const std::string& value) {
  if (!value.empty()) q.emplace_back(key, value);
}

void push(Query& q, const char* key, const std::optional<std::string>& value) {
  if (value && !value->empty()) q.emplace_back(key, *value);
}

/// An error status into Error (Api) with the API's detail.
void check(const http::Response& r) {
  if (r.status < 400) return;
  std::string detail = parse::error_detail(r.body);
  if (detail.empty()) detail = http::reason(r.status);
  throw Error(ErrorKind::Api, static_cast<int>(r.status), detail);
}

/// dest/<object_path>, refusing a path that is absolute or climbs out of dest.
fs::path target_path(const fs::path& dest, const std::string& object_path) {
  bool ok = !object_path.empty() && object_path.find('\\') == std::string::npos &&
            object_path.find(':') == std::string::npos && object_path.front() != '/';
  fs::path rel = fs::u8path(object_path);
  for (const auto& part : rel) {
    if (part.string() == "..") ok = false;
  }
  if (!ok || rel.is_absolute() || rel.has_root_path()) {
    throw Error(ErrorKind::Invalid, 0,
                "refusing object path \"" + object_path +
                    "\": not a relative path inside the destination");
  }
  return dest / rel;
}

bool iequals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    char x = a[i], y = b[i];
    if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
    if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
    if (x != y) return false;
  }
  return true;
}

}  // namespace

const char* version() noexcept { return PROBNODE_VERSION; }

Client::Client() : Client(ClientOptions{}) {}

Client::Client(ClientOptions options) : timeout_seconds_(options.timeout_seconds) {
  if (options.from_env && (!options.api_key || !options.base_url)) {
    Config saved = Config::load();
    if (!options.api_key) {
      options.api_key = env("PROBNODE_API_KEY");
      if (!options.api_key) options.api_key = saved.api_key;
    }
    if (!options.base_url) {
      options.base_url = env("PROBNODE_BASE_URL");
      if (!options.base_url) options.base_url = saved.base_url;
    }
  }
  api_key_ = options.api_key.value_or("");
  base_url_ = options.base_url.value_or(kDefaultBaseUrl);
  while (!base_url_.empty() && base_url_.back() == '/') base_url_.pop_back();
}

// -- plumbing ---------------------------------------------------------------------------------

std::string Client::get(const std::string& path, const Query& query, bool need_key) const {
  if (need_key && api_key_.empty()) throw Error(ErrorKind::NoApiKey, 401, "no API key: pass one, set PROBNODE_API_KEY, or run `probnode login`");
  http::Request r;
  r.url = base_url_ + path + http::query_string(query);
  r.timeout_seconds = timeout_seconds_;
  if (!api_key_.empty()) r.headers.push_back("Authorization: Bearer " + api_key_);
  auto response = http::send(r);
  check(response);
  return std::move(response.body);
}

std::string Client::post(const std::string& path, const std::string& json_body,
                         bool need_key) const {
  if (need_key && api_key_.empty()) throw Error(ErrorKind::NoApiKey, 401, "no API key: pass one, set PROBNODE_API_KEY, or run `probnode login`");
  http::Request r;
  r.method = "POST";
  r.url = base_url_ + path;
  r.body = json_body;
  r.timeout_seconds = timeout_seconds_;
  if (!api_key_.empty()) r.headers.push_back("Authorization: Bearer " + api_key_);
  auto response = http::send(r);
  check(response);
  return std::move(response.body);
}

// -- account ----------------------------------------------------------------------------------

std::string Client::login(const std::string& email, const std::string& password) {
  json body = {{"email", email}, {"password", password}};
  json out = parse::document(post("/v1/auth/login", body.dump(), false), "login");
  if (!out.is_object() || !out.contains("key") || !out["key"].is_string()) {
    throw Error(ErrorKind::Json, 0, "the login answer has no key");
  }
  api_key_ = out["key"].get<std::string>();
  return api_key_;
}

User Client::me() const { return parse::user(parse::document(get("/v1/me", {}, true), "me")); }

Usage Client::usage() const {
  return parse::usage(parse::document(get("/v1/data/usage", {}, true), "usage"));
}

// -- catalog ----------------------------------------------------------------------------------

std::vector<Dataset> Client::datasets() const {
  return parse::list<Dataset>(parse::document(get("/v1/data/datasets", {}, false), "datasets"),
                              parse::dataset);
}

std::vector<Series> Client::series(const std::string& category, const std::string& q) const {
  Query query;
  push(query, "category", category);
  push(query, "q", q);
  return parse::list<Series>(parse::document(get("/v1/catalog/series", query, false), "series"),
                             parse::series);
}

std::vector<Market> Client::markets(const MarketQuery& mq) const {
  Query query;
  push(query, "series", mq.series);
  push(query, "category", mq.category);
  push(query, "q", mq.q);
  push(query, "status", mq.status);
  if (mq.limit) query.emplace_back("limit", std::to_string(*mq.limit));
  if (mq.offset) query.emplace_back("offset", std::to_string(*mq.offset));
  return parse::list<Market>(parse::document(get("/v1/catalog/markets", query, false), "markets"),
                             parse::market);
}

std::vector<CoverageHour> Client::coverage(const std::string& token_id, const std::string& start,
                                           const std::string& end) const {
  Query query{{"token_id", token_id}};
  push(query, "start", start);
  push(query, "end", end);
  return parse::list<CoverageHour>(
      parse::document(get("/v1/catalog/coverage", query, false), "coverage"),
      parse::coverage_hour);
}

std::vector<Sample> Client::samples() const {
  return parse::list<Sample>(parse::document(get("/v1/samples", {}, false), "samples"),
                             parse::sample);
}

// -- files ------------------------------------------------------------------------------------

std::vector<DataFile> Client::files(const std::string& dataset, const std::string& format,
                                    const std::string& start, const std::string& end,
                                    const FilesQuery& fq) const {
  Query query{{"dataset", dataset}, {"format", format}, {"start", start}, {"end", end}};
  if (!fq.granularity.empty() && fq.granularity != "hour") {
    query.emplace_back("granularity", fq.granularity);
  }
  for (const auto& m : fq.markets) query.emplace_back("market", m);
  for (const auto& s : fq.series) query.emplace_back("series", s);
  json out = parse::document(get("/v1/data/files", query, true), "files");
  if (!out.is_object() || !out.contains("files")) {
    throw Error(ErrorKind::Json, 0, "the files answer has no files");
  }
  return parse::list<DataFile>(out["files"], parse::data_file);
}

std::map<std::string, std::string> Client::sign(const std::vector<std::string>& paths) const {
  std::map<std::string, std::string> urls;
  for (std::size_t i = 0; i < paths.size(); i += kSignBatch) {
    std::size_t end = std::min(paths.size(), i + kSignBatch);
    json body = {{"paths", std::vector<std::string>(paths.begin() + static_cast<std::ptrdiff_t>(i),
                                                    paths.begin() + static_cast<std::ptrdiff_t>(end))}};
    json out = parse::document(post("/v1/data/urls", body.dump(), true), "signed urls");
    if (!out.is_object() || !out.contains("urls")) {
      throw Error(ErrorKind::Json, 0, "the signing answer has no urls");
    }
    for (const auto& u : parse::list<SignedUrl>(out["urls"], parse::signed_url)) {
      urls[u.object_path] = u.url;
    }
  }
  return urls;
}

fs::path Client::fetch(const std::string& object_path, const std::string& sha256,
                       const std::string& url, const fs::path& dest, bool verify) const {
  fs::path target = target_path(dest, object_path);
  std::error_code ec;
  if (target.has_parent_path()) fs::create_directories(target.parent_path(), ec);
  if (ec) throw Error(ErrorKind::Io, 0, "cannot create " + target.parent_path().string());
  fs::path part = target;
  part += ".part";

  Sha256 hasher;
  long status = 0;
  {
    std::ofstream out(part, std::ios::binary | std::ios::trunc);
    if (!out) throw Error(ErrorKind::Io, 0, "cannot write " + part.string());
    http::Request r;
    r.url = url;
    r.timeout_seconds = timeout_seconds_;
    r.follow_redirects = true;  // the link carries its own authorisation; no key is sent
    try {
      status = http::stream(r, [&](const char* p, std::size_t n) {
        hasher.update(p, n);
        out.write(p, static_cast<std::streamsize>(n));
        if (!out) throw Error(ErrorKind::Io, 0, "cannot write " + part.string());
      });
    } catch (...) {
      out.close();
      fs::remove(part, ec);
      throw;
    }
    out.close();
    if (!out) {
      fs::remove(part, ec);
      throw Error(ErrorKind::Io, 0, "cannot write " + part.string());
    }
  }
  if (status >= 400) {
    fs::remove(part, ec);
    throw Error(ErrorKind::Api, static_cast<int>(status), "fetching " + object_path + " failed");
  }
  std::string actual = hasher.hex();
  if (verify && !iequals(actual, sha256)) {
    fs::remove(part, ec);
    throw Error(ErrorKind::Checksum, 0,
                "sha256 mismatch for " + object_path + ": expected " + sha256 + ", got " + actual);
  }
  fs::rename(part, target, ec);
  if (ec) {
    fs::remove(part, ec);
    throw Error(ErrorKind::Io, 0, "cannot rename to " + target.string());
  }
  return target;
}

fs::path Client::download(const DataFile& file, const fs::path& dest,
                          const DownloadOptions& options) const {
  std::string url;
  if (options.url) {
    url = *options.url;
  } else {
    auto urls = sign({file.object_path});
    auto it = urls.find(file.object_path);
    if (it == urls.end()) throw Error(ErrorKind::Invalid, 0, "no signed link for " + file.object_path);
    url = it->second;
  }
  return fetch(file.object_path, file.sha256, url, dest, options.verify);
}

fs::path Client::download(const BookExport& day, const fs::path& dest,
                          const DownloadOptions& options) const {
  if (day.urls.empty() && !options.url) throw Error(ErrorKind::Invalid, 0, "the export has no link");
  std::string object_path = day.object_path();
  std::string url = options.url ? *options.url : day.urls.front().url;
  return fetch(object_path, day.sha256, url, dest, options.verify);
}

fs::path Client::download(const Sample& sample, const fs::path& dest,
                          const DownloadOptions& options) const {
  std::string url = options.url ? *options.url : sample.url;
  return fetch(sample.file.object_path, sample.file.sha256, url, dest, options.verify);
}

std::vector<fs::path> Client::download_all(const std::vector<DataFile>& files,
                                           const fs::path& dest) const {
  std::vector<std::string> paths;
  paths.reserve(files.size());
  for (const auto& f : files) paths.push_back(f.object_path);
  auto urls = sign(paths);
  std::vector<fs::path> out;
  out.reserve(files.size());
  for (const auto& f : files) {
    DownloadOptions options;
    if (auto it = urls.find(f.object_path); it != urls.end()) options.url = it->second;
    out.push_back(download(f, dest, options));
  }
  return out;
}

BookExport Client::export_book(const std::string& market, const std::string& day,
                               const ExportOptions& options) const {
  json body = {{"market", market},
               {"day", day},
               {"schema", options.schema},
               {"format", options.format}};
  BookExport out =
      parse::book_export(parse::document(post("/v1/data/exports", body.dump(), true), "export"));
  if (out.urls.size() != 1) {
    throw Error(ErrorKind::Invalid, 0,
                "the export answered with " + std::to_string(out.urls.size()) +
                    " links, expected one");
  }
  return out;
}

// -- range query ------------------------------------------------------------------------------

std::string Client::timeseries(const std::string& dataset, const std::string& market,
                               const std::string& start, const std::string& end) const {
  return get("/v1/timeseries",
             {{"dataset", dataset},
              {"market", market},
              {"start", start},
              {"end", end},
              {"format", "parquet"}},
             true);
}

std::vector<std::string> Client::timeseries_ndjson(const std::string& dataset,
                                                   const std::string& market,
                                                   const std::string& start,
                                                   const std::string& end) const {
  std::string text = get("/v1/timeseries",
                         {{"dataset", dataset},
                          {"market", market},
                          {"start", start},
                          {"end", end},
                          {"format", "ndjson"}},
                         true);
  std::vector<std::string> rows;
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.find_first_not_of(" \t") != std::string::npos) rows.push_back(line);
  }
  return rows;
}

}  // namespace probnode
