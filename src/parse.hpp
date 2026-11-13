// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
//
// JSON answers into the public structs. Private to the library.
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "probnode/error.hpp"
#include "probnode/models.hpp"

namespace probnode::parse {

using nlohmann::json;

/// Parses text as JSON; throws Error (Json) naming `what` when it is not.
json document(const std::string& text, const char* what);

Dataset dataset(const json& j);
Series series(const json& j);
Market market(const json& j);
CoverageHour coverage_hour(const json& j);
DataFile data_file(const json& j);
Sample sample(const json& j);
SignedUrl signed_url(const json& j);
BookExport book_export(const json& j);
User user(const json& j);
Usage usage(const json& j);

/// Each element of a JSON array through `one`; throws Error (Json) if `j` is not an array.
template <class T, class F>
std::vector<T> list(const json& j, F one) {
  std::vector<T> out;
  if (!j.is_array()) throw Error(ErrorKind::Json, 0, "expected a JSON array");
  out.reserve(j.size());
  for (const auto& item : j) out.push_back(one(item));
  return out;
}

/// The API's `detail` of an error body: the string, the JSON of anything else, or "" if none.
std::string error_detail(const std::string& body);

}  // namespace probnode::parse
