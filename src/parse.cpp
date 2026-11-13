// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
#include "parse.hpp"

#include "probnode/error.hpp"

namespace probnode::parse {
namespace {

std::string str(const json& j, const char* key) {
  auto it = j.find(key);
  if (it == j.end() || it->is_null()) return {};
  if (it->is_string()) return it->get<std::string>();
  return it->dump();
}

std::optional<std::string> opt_str(const json& j, const char* key) {
  auto it = j.find(key);
  if (it == j.end() || it->is_null()) return std::nullopt;
  if (it->is_string()) return it->get<std::string>();
  return it->dump();
}

template <class T>
T num(const json& j, const char* key, T fallback = T{}) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_number()) return fallback;
  return it->get<T>();
}

template <class T>
std::optional<T> opt_num(const json& j, const char* key) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_number()) return std::nullopt;
  return it->get<T>();
}

bool boolean(const json& j, const char* key) {
  auto it = j.find(key);
  return it != j.end() && it->is_boolean() && it->get<bool>();
}

std::vector<std::string> strings(const json& j, const char* key) {
  std::vector<std::string> out;
  auto it = j.find(key);
  if (it == j.end() || !it->is_array()) return out;
  for (const auto& v : *it) {
    if (v.is_string()) out.push_back(v.get<std::string>());
  }
  return out;
}

void require_object(const json& j, const char* what) {
  if (!j.is_object()) throw Error(ErrorKind::Json, 0, std::string("expected an object: ") + what);
}

}  // namespace

json document(const std::string& text, const char* what) {
  try {
    return json::parse(text);
  } catch (const json::exception& e) {
    throw Error(ErrorKind::Json, 0, std::string("not JSON (") + what + "): " + e.what());
  }
}

Dataset dataset(const json& j) {
  require_object(j, "dataset");
  Dataset d;
  d.code = str(j, "code");
  d.title = str(j, "title");
  d.formats = strings(j, "formats");
  d.market_day_formats = strings(j, "market_day_formats");
  d.tier = str(j, "tier");
  d.first_hour = opt_str(j, "first_hour");
  d.last_hour = opt_str(j, "last_hour");
  d.schema = opt_str(j, "schema");
  d.aliases = strings(j, "aliases");
  d.level = opt_str(j, "level");
  d.kind = opt_str(j, "kind");
  d.blurb = opt_str(j, "blurb");
  d.tags = strings(j, "tags");
  d.legacy = boolean(j, "legacy");
  return d;
}

Series series(const json& j) {
  require_object(j, "series");
  return Series{str(j, "slug"), str(j, "title"), str(j, "venue"), str(j, "category")};
}

Market market(const json& j) {
  require_object(j, "market");
  Market m;
  m.slug = str(j, "slug");
  m.series_slug = opt_str(j, "series_slug");
  m.start_ts = str(j, "start_ts");
  m.end_ts = str(j, "end_ts");
  m.resolution = opt_str(j, "resolution");
  if (auto it = j.find("tokens"); it != j.end() && it->is_array()) {
    for (const auto& t : *it) {
      if (t.is_object()) m.tokens.push_back(Token{str(t, "token_id"), str(t, "outcome")});
    }
  }
  return m;
}

CoverageHour coverage_hour(const json& j) {
  require_object(j, "coverage hour");
  CoverageHour c;
  c.token_id = str(j, "token_id");
  c.hour_start = str(j, "hour_start");
  c.recorded_ms = num<std::int64_t>(j, "recorded_ms");
  c.gap_count = num<std::int64_t>(j, "gap_count");
  c.uptime_pct = num<double>(j, "uptime_pct");
  c.implied = boolean(j, "implied");
  c.derived_ms = num<std::int64_t>(j, "derived_ms");
  c.era = opt_str(j, "era");
  c.era_split_ms = opt_num<std::int64_t>(j, "era_split_ms");
  c.clock_source = opt_str(j, "clock_source");
  c.clock_stratum = opt_num<std::int64_t>(j, "clock_stratum");
  c.clock_synced_pct = opt_num<double>(j, "clock_synced_pct");
  c.clock_offset_max_us = opt_num<std::int64_t>(j, "clock_offset_max_us");
  c.clock_error_max_us = opt_num<std::int64_t>(j, "clock_error_max_us");
  return c;
}

DataFile data_file(const json& j) {
  require_object(j, "file");
  DataFile f;
  f.object_path = str(j, "object_path");
  f.dataset = str(j, "dataset");
  f.format = str(j, "format");
  f.event_date = str(j, "event_date");
  f.hour = num<int>(j, "hour");
  f.src = str(j, "src");
  f.bytes = num<std::uint64_t>(j, "bytes");
  f.rows = num<std::uint64_t>(j, "rows");
  f.sha256 = str(j, "sha256");
  f.granularity = str(j, "granularity");
  f.market = opt_str(j, "market");
  f.era = opt_str(j, "era");
  return f;
}

Sample sample(const json& j) { return Sample{data_file(j), str(j, "url")}; }

SignedUrl signed_url(const json& j) {
  require_object(j, "signed url");
  return SignedUrl{str(j, "object_path"), str(j, "url"), str(j, "expires_at")};
}

BookExport book_export(const json& j) {
  require_object(j, "export");
  BookExport e;
  e.schema = str(j, "schema");
  e.rows = num<std::uint64_t>(j, "rows");
  e.bytes = num<std::uint64_t>(j, "bytes");
  e.sha256 = str(j, "sha256");
  e.built = boolean(j, "built");
  if (auto it = j.find("urls"); it != j.end()) e.urls = list<SignedUrl>(*it, signed_url);
  e.licence = str(j, "licence");
  e.licence_detail = str(j, "licence_detail");
  return e;
}

User user(const json& j) {
  require_object(j, "user");
  User u;
  u.id = str(j, "id");
  u.email = str(j, "email");
  u.plan = str(j, "plan");
  u.plan_expires_at = opt_str(j, "plan_expires_at");
  u.email_verified = boolean(j, "email_verified");
  u.created_at = str(j, "created_at");
  return u;
}

Usage usage(const json& j) {
  require_object(j, "usage");
  Usage u;
  u.plan = str(j, "plan");
  u.datasets = strings(j, "datasets");
  u.signed_urls_used_today = num<std::uint64_t>(j, "signed_urls_used_today");
  u.signed_urls_per_day = num<std::uint64_t>(j, "signed_urls_per_day");
  u.requests_per_minute = num<std::uint64_t>(j, "requests_per_minute");
  u.resets_at = str(j, "resets_at");
  u.freshness = str(j, "freshness");
  u.freshness_cutoff = opt_str(j, "freshness_cutoff");
  return u;
}

std::string error_detail(const std::string& body) {
  json j = json::parse(body, nullptr, /*allow_exceptions=*/false);
  if (!j.is_object()) return {};
  auto it = j.find("detail");
  if (it == j.end() || it->is_null()) return {};
  if (it->is_string()) return it->get<std::string>();
  return it->dump();
}

}  // namespace probnode::parse
