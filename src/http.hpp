// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
//
// The libcurl calls, behind a small interface. Private to the library.
#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace probnode::http {

struct Response {
  long status = 0;
  std::string body;
};

struct Request {
  std::string method = "GET";
  std::string url;
  std::vector<std::string> headers;  // "Name: value"
  std::string body;
  long timeout_seconds = 60;
  bool follow_redirects = false;
};

/// Sends the request; throws Error (Transport) when it cannot be made.
Response send(const Request& request);

/// Sends a GET, streaming the body into `sink` when the status is below 400 (the body of an
/// error status is not streamed). Returns the status. Throws Error (Transport) on failure, or
/// whatever `sink` throws.
long stream(const Request& request, const std::function<void(const char*, std::size_t)>& sink);

/// Percent-encodes a query component (RFC 3986 unreserved characters kept).
std::string escape(const std::string& value);

/// "?k=v&k=v" (empty when there are none).
std::string query_string(const std::vector<std::pair<std::string, std::string>>& query);

/// The reason phrase of a status ("Service Unavailable").
const char* reason(long status);

}  // namespace probnode::http
