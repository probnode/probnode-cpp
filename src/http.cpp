// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
#include "http.hpp"

#include <curl/curl.h>

#include <exception>
#include <memory>
#include <mutex>

#include "probnode/error.hpp"

#ifndef PROBNODE_VERSION
#define PROBNODE_VERSION "0.0.0"
#endif

namespace probnode::http {
namespace {

void global_init() {
  static std::once_flag once;
  std::call_once(once, [] {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
      throw Error(ErrorKind::Transport, 0, "libcurl could not be initialised");
    }
  });
}

struct Easy {
  CURL* handle = nullptr;
  curl_slist* headers = nullptr;
  Easy() {
    global_init();
    handle = curl_easy_init();
    if (handle == nullptr) throw Error(ErrorKind::Transport, 0, "libcurl: no handle");
  }
  ~Easy() {
    if (headers != nullptr) curl_slist_free_all(headers);
    curl_easy_cleanup(handle);
  }
  Easy(const Easy&) = delete;
  Easy& operator=(const Easy&) = delete;
};

void setup(Easy& e, const Request& r) {
  CURL* h = e.handle;
  curl_easy_setopt(h, CURLOPT_URL, r.url.c_str());
  curl_easy_setopt(h, CURLOPT_USERAGENT, "probnode-cpp/" PROBNODE_VERSION);
  curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(h, CURLOPT_TIMEOUT, r.timeout_seconds);
  curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 30L);
  curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, r.follow_redirects ? 1L : 0L);
  curl_easy_setopt(h, CURLOPT_MAXREDIRS, 10L);
  for (const auto& line : r.headers) e.headers = curl_slist_append(e.headers, line.c_str());
  if (r.method == "POST") {
    e.headers = curl_slist_append(e.headers, "Content-Type: application/json");
    curl_easy_setopt(h, CURLOPT_POST, 1L);
    curl_easy_setopt(h, CURLOPT_POSTFIELDS, r.body.c_str());
    curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(r.body.size()));
  } else if (r.method != "GET") {
    curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, r.method.c_str());
  }
  e.headers = curl_slist_append(e.headers, "Accept: application/json, */*");
  curl_easy_setopt(h, CURLOPT_HTTPHEADER, e.headers);
}

void perform(Easy& e, const std::string& url) {
  CURLcode rc = curl_easy_perform(e.handle);
  if (rc != CURLE_OK) {
    throw Error(ErrorKind::Transport, 0,
                std::string("request to ") + url + " failed: " + curl_easy_strerror(rc));
  }
}

size_t to_string(char* p, size_t size, size_t n, void* out) {
  static_cast<std::string*>(out)->append(p, size * n);
  return size * n;
}

struct Sink {
  CURL* handle;
  const std::function<void(const char*, std::size_t)>* fn;
  std::exception_ptr error;
};

size_t to_sink(char* p, size_t size, size_t n, void* user) {
  auto* s = static_cast<Sink*>(user);
  long status = 0;
  curl_easy_getinfo(s->handle, CURLINFO_RESPONSE_CODE, &status);
  if (status >= 400) return size * n;  // an error body is dropped
  try {
    (*s->fn)(p, size * n);
  } catch (...) {
    s->error = std::current_exception();
    return 0;  // aborts the transfer
  }
  return size * n;
}

}  // namespace

Response send(const Request& request) {
  Easy e;
  setup(e, request);
  Response out;
  curl_easy_setopt(e.handle, CURLOPT_WRITEFUNCTION, to_string);
  curl_easy_setopt(e.handle, CURLOPT_WRITEDATA, &out.body);
  perform(e, request.url);
  curl_easy_getinfo(e.handle, CURLINFO_RESPONSE_CODE, &out.status);
  return out;
}

long stream(const Request& request, const std::function<void(const char*, std::size_t)>& sink) {
  Easy e;
  setup(e, request);
  Sink s{e.handle, &sink, nullptr};
  curl_easy_setopt(e.handle, CURLOPT_WRITEFUNCTION, to_sink);
  curl_easy_setopt(e.handle, CURLOPT_WRITEDATA, &s);
  CURLcode rc = curl_easy_perform(e.handle);
  if (s.error) std::rethrow_exception(s.error);
  if (rc != CURLE_OK) {
    throw Error(ErrorKind::Transport, 0,
                std::string("fetching ") + request.url + " failed: " + curl_easy_strerror(rc));
  }
  long status = 0;
  curl_easy_getinfo(e.handle, CURLINFO_RESPONSE_CODE, &status);
  return status;
}

std::string escape(const std::string& value) {
  static const char* hex = "0123456789ABCDEF";
  std::string out;
  out.reserve(value.size());
  for (unsigned char c : value) {
    bool keep = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~';
    if (keep) {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 15]);
    }
  }
  return out;
}

std::string query_string(const std::vector<std::pair<std::string, std::string>>& query) {
  std::string out;
  for (const auto& [k, v] : query) {
    out += out.empty() ? '?' : '&';
    out += escape(k);
    out += '=';
    out += escape(v);
  }
  return out;
}

const char* reason(long status) {
  switch (status) {
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 402: return "Payment Required";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 408: return "Request Timeout";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 413: return "Payload Too Large";
    case 422: return "Unprocessable Entity";
    case 429: return "Too Many Requests";
    case 500: return "Internal Server Error";
    case 501: return "Not Implemented";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    case 504: return "Gateway Timeout";
    default: return "HTTP error";
  }
}

}  // namespace probnode::http
