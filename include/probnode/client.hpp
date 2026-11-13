// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
//
// The official C++ client for the ProbNode API.
//
//   #include <probnode/client.hpp>
//
//   probnode::Client pn;  // PROBNODE_API_KEY, or the key `probnode login` saved
//   probnode::ExportOptions opts;
//   opts.schema = "book-max";
//   opts.format = "parquet";
//   auto day = pn.export_book("will-x-happen", "2026-11-12", opts);
//   auto path = pn.download(day, "data/");  // sha256-checked
//   std::cout << day.rows << " rows in " << path << "\n";
//
// With C++20, the options read pn.export_book(m, d, {.schema = "book-max", .format = "parquet"}).
//
// Every method is one HTTP call, except download() (sign, then fetch) and sign() (one call per
// 100 paths). Failures throw probnode::Error with the HTTP status and the API's own sentence.
#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "probnode/config.hpp"
#include "probnode/error.hpp"
#include "probnode/models.hpp"
#include "probnode/sha256.hpp"

namespace probnode {

/// The production API.
inline constexpr const char* kDefaultBaseUrl = "https://probnode.com";
/// POST /v1/data/urls signs at most this many paths a request; Client::sign() batches.
inline constexpr std::size_t kSignBatch = 100;

/// The library version, "0.1.0".
const char* version() noexcept;

/// How a Client is made. Unset fields come from the environment when `from_env` is true.
struct ClientOptions {
  /// The API key. Else PROBNODE_API_KEY, else the key `probnode login` saved.
  std::optional<std::string> api_key;
  /// The API base URL. Else PROBNODE_BASE_URL, else the saved one, else kDefaultBaseUrl.
  std::optional<std::string> base_url;
  /// Seconds each request may take, a whole download included (0: no limit).
  long timeout_seconds = 60;
  /// Fill unset fields from the environment and the saved config.
  bool from_env = true;
};

/// Narrows Client::files().
struct FilesQuery {
  std::string granularity = "hour";  ///< "hour" or "market_day"
  std::vector<std::string> markets;  ///< market slugs
  std::vector<std::string> series;   ///< series slugs
};

/// What Client::export_book() builds.
struct ExportOptions {
  std::string schema = "book-max";  ///< one of kMarketDaySchemas
  std::string format = "parquet";   ///< "parquet" or "csv" (gzip-compressed)
};

/// How Client::download() fetches.
struct DownloadOptions {
  bool verify = true;              ///< check the sha256 against the listing
  std::optional<std::string> url;  ///< a link signed beforehand
};

/// A session against one ProbNode deployment. Public calls (datasets, the catalog, coverage,
/// samples) work without a key. A Client may be used from several threads at once.
class Client {
 public:
  /// From the environment: see ClientOptions.
  Client();
  explicit Client(ClientOptions options);

  /// The API base URL, without a trailing slash.
  const std::string& base_url() const noexcept { return base_url_; }
  /// The key in use; empty when there is none.
  const std::string& api_key() const noexcept { return api_key_; }

  // -- account --------------------------------------------------------------------------------

  /// Mints a key with email and password; the client uses it from then on. Save it with
  /// Config to share it with the `probnode` command line.
  std::string login(const std::string& email, const std::string& password);
  User me() const;
  Usage usage() const;

  // -- catalog --------------------------------------------------------------------------------

  /// Every dataset, its formats, its tier and the hours held. Public.
  std::vector<Dataset> datasets() const;
  /// Series, optionally of one category and matching a substring (empty: no filter). Public.
  std::vector<Series> series(const std::string& category = "", const std::string& q = "") const;
  /// Markets, filtered. Public.
  std::vector<Market> markets(const MarketQuery& query = {}) const;
  /// The recorded hours of one token over an optional window. Public.
  std::vector<CoverageHour> coverage(const std::string& token_id, const std::string& start = "",
                                     const std::string& end = "") const;
  /// Free sample files with their links. Public.
  std::vector<Sample> samples() const;

  // -- files ----------------------------------------------------------------------------------

  /// The files of one dataset and format over [start, end) (ISO times on the hour).
  std::vector<DataFile> files(const std::string& dataset, const std::string& format,
                              const std::string& start, const std::string& end,
                              const FilesQuery& query = {}) const;

  /// Signed links for these object paths: object_path -> url. One request per 100 paths.
  std::map<std::string, std::string> sign(const std::vector<std::string>& paths) const;

  /// Fetches one file to dest/<object_path> and returns that path. The bytes go to a .part file
  /// first, renamed only when complete and its sha256 matches; on a mismatch the .part is
  /// removed and Error (Checksum) thrown.
  std::filesystem::path download(const DataFile& file, const std::filesystem::path& dest,
                                 const DownloadOptions& options = {}) const;
  /// An export, through its own signed link.
  std::filesystem::path download(const BookExport& day, const std::filesystem::path& dest,
                                 const DownloadOptions& options = {}) const;
  /// A sample, through its own link; needs no key.
  std::filesystem::path download(const Sample& sample, const std::filesystem::path& dest,
                                 const DownloadOptions& options = {}) const;
  /// Every file, their links signed in batches of 100 first. Stops at the first failure.
  std::vector<std::filesystem::path> download_all(const std::vector<DataFile>& files,
                                                  const std::filesystem::path& dest) const;

  /// One market's UTC day of a schema (default book-max, the full book) as parquet or csv.gz,
  /// made on the first request and found by later ones. Pass the answer to download(). The day
  /// must have ended (UTC). Needs a plan that covers the schema.
  BookExport export_book(const std::string& market, const std::string& day,
                         const ExportOptions& options = {}) const;

  // -- range query ----------------------------------------------------------------------------

  /// Rows of one market over a window (at most 7 days, 200,000 rows) as parquet bytes.
  std::string timeseries(const std::string& dataset, const std::string& market,
                         const std::string& start, const std::string& end) const;
  /// The same rows as NDJSON: one JSON object (text) per row.
  std::vector<std::string> timeseries_ndjson(const std::string& dataset, const std::string& market,
                                             const std::string& start,
                                             const std::string& end) const;

 private:
  std::string get(const std::string& path,
                  const std::vector<std::pair<std::string, std::string>>& query,
                  bool need_key) const;
  std::string post(const std::string& path, const std::string& json_body, bool need_key) const;
  std::filesystem::path fetch(const std::string& object_path, const std::string& sha256,
                              const std::string& url, const std::filesystem::path& dest,
                              bool verify) const;

  std::string api_key_;
  std::string base_url_;
  long timeout_seconds_;
};

}  // namespace probnode
