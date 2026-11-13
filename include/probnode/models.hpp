// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
//
// What the API returns, as plain structs. Times are the API's ISO 8601 strings (UTC); a field the
// API may leave null is a std::optional.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace probnode {

/// The schemas POST /v1/data/exports builds for one market's UTC day. book-max is the default;
/// book_full is accepted as its alias.
inline constexpr std::array<std::string_view, 15> kMarketDaySchemas = {
    "book-max", "book-10",      "book-top", "book-top-1s", "book-top-1m",
    "trades",   "trade-quotes", "pair",     "fills",       "bars-1s",
    "bars-1m",  "bars-1h",      "bars-1d",  "market-stats", "market-events",
};

/// A dataset, its formats, its tier and the hours held (GET /v1/data/datasets).
struct Dataset {
  std::string code;   ///< as files() takes it: book_full, trades, ...
  std::string title;
  std::vector<std::string> formats;             ///< formats of its hourly files
  std::vector<std::string> market_day_formats;  ///< formats of its market-day files
  std::string tier;
  std::optional<std::string> first_hour;
  std::optional<std::string> last_hour;
  std::optional<std::string> schema;
  std::vector<std::string> aliases;
  std::optional<std::string> level;
  std::optional<std::string> kind;
  std::optional<std::string> blurb;
  std::vector<std::string> tags;
  bool legacy = false;
};

/// A series of markets (GET /v1/catalog/series).
struct Series {
  std::string slug;
  std::string title;
  std::string venue;
  std::string category;
};

/// One outcome token of a market.
struct Token {
  std::string token_id;
  std::string outcome;
};

/// A market (GET /v1/catalog/markets).
struct Market {
  std::string slug;  ///< as files(), export_book() and timeseries() take it
  std::optional<std::string> series_slug;
  std::string start_ts;
  std::string end_ts;
  std::optional<std::string> resolution;
  std::vector<Token> tokens;
};

/// Filters for Client::markets(). Every field is optional.
struct MarketQuery {
  std::optional<std::string> series;
  std::optional<std::string> category;
  std::optional<std::string> q;       ///< a substring of the slug
  std::optional<std::string> status;  ///< open or resolved
  std::optional<int> limit;           ///< page size (API default 100, at most 1000)
  std::optional<int> offset;
};

/// One token's hour of recording (GET /v1/catalog/coverage).
struct CoverageHour {
  std::string token_id;
  std::string hour_start;
  std::int64_t recorded_ms = 0;
  std::int64_t gap_count = 0;
  double uptime_pct = 0;
  bool implied = false;
  std::int64_t derived_ms = 0;
  std::optional<std::string> era;
  std::optional<std::int64_t> era_split_ms;
  std::optional<std::string> clock_source;
  std::optional<std::int64_t> clock_stratum;
  std::optional<double> clock_synced_pct;
  std::optional<std::int64_t> clock_offset_max_us;
  std::optional<std::int64_t> clock_error_max_us;
};

/// One file in the store, as GET /v1/data/files lists it.
struct DataFile {
  std::string object_path;  ///< also where download() puts it under the destination
  std::string dataset;
  std::string format;
  std::string event_date;  ///< YYYY-MM-DD
  int hour = 0;
  std::string src;
  std::uint64_t bytes = 0;
  std::uint64_t rows = 0;
  std::string sha256;  ///< lowercase hex
  std::string granularity;
  std::optional<std::string> market;
  std::optional<std::string> era;
};

/// A free sample file with its link (GET /v1/samples). Downloads without a key.
struct Sample {
  DataFile file;
  std::string url;
};

/// A signed link to one object, valid for minutes. Fetch it without the API key.
struct SignedUrl {
  std::string object_path;
  std::string url;
  std::string expires_at;
};

/// One market's UTC day of a schema, made or found by POST /v1/data/exports.
struct BookExport {
  std::string schema;  ///< the schema the file holds (book-max for book_full)
  std::uint64_t rows = 0;
  std::uint64_t bytes = 0;
  std::string sha256;
  bool built = false;  ///< true when this request made the file
  std::vector<SignedUrl> urls;
  std::string licence;
  std::string licence_detail;

  /// The file's path in the store (empty if the answer had no link).
  std::string object_path() const { return urls.empty() ? std::string() : urls.front().object_path; }
};

/// The account (GET /v1/me).
struct User {
  std::string id;
  std::string email;
  std::string plan;
  std::optional<std::string> plan_expires_at;
  bool email_verified = false;
  std::string created_at;
};

/// The plan's limits and today's use (GET /v1/data/usage).
struct Usage {
  std::string plan;
  std::vector<std::string> datasets;
  std::uint64_t signed_urls_used_today = 0;
  std::uint64_t signed_urls_per_day = 0;
  std::uint64_t requests_per_minute = 0;
  std::string resets_at;
  std::string freshness;
  std::optional<std::string> freshness_cutoff;
};

}  // namespace probnode
