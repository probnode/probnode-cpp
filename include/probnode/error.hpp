// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace probnode {

/// What kind of failure an Error is.
enum class ErrorKind {
  /// The API answered with an error status; status() and detail() say which and why.
  Api,
  /// A call that needs an API key was made without one. Nothing was sent.
  NoApiKey,
  /// A downloaded file's sha256 is not the one the API listed. The partial file was removed.
  Checksum,
  /// The request could not be made (DNS, TLS, connection, timeout).
  Transport,
  /// Reading or writing a local file failed.
  Io,
  /// A response or the saved config was not the JSON expected.
  Json,
  /// An argument or a response the client refuses, such as an object path that would leave the
  /// destination directory.
  Invalid,
};

/// Every failure of the client. what() is "<status>: <detail>" for an API error (for example
/// "402: your plan does not include book-max"), and the detail alone otherwise.
class Error : public std::runtime_error {
 public:
  Error(ErrorKind kind, int status, std::string detail)
      : std::runtime_error(status > 0 ? std::to_string(status) + ": " + detail : detail),
        kind_(kind),
        status_(status),
        detail_(std::move(detail)) {}

  /// What kind of failure this is.
  ErrorKind kind() const noexcept { return kind_; }
  /// The HTTP status of an API error (401 for NoApiKey); 0 when there is none.
  int status() const noexcept { return status_; }
  /// The API's own sentence, or the client's description.
  const std::string& detail() const noexcept { return detail_; }

 private:
  ErrorKind kind_;
  int status_;
  std::string detail_;
};

}  // namespace probnode
