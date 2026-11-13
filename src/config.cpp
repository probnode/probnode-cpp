// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
#include "probnode/config.hpp"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <system_error>

#include "probnode/error.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

namespace probnode {
namespace {

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

json read_json(const fs::path& file, bool* exists) {
  std::ifstream in(file, std::ios::binary);
  *exists = static_cast<bool>(in);
  if (!in) return json::object();
  std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  try {
    return json::parse(text);
  } catch (const json::exception& e) {
    throw Error(ErrorKind::Json, 0, "the config " + file.string() + " is not JSON: " + e.what());
  }
}

}  // namespace

std::optional<fs::path> Config::path() {
  if (auto p = env("PROBNODE_CONFIG")) return fs::u8path(*p);
#ifdef _WIN32
  auto home = env("USERPROFILE");
  if (!home) home = env("HOME");
#else
  auto home = env("HOME");
#endif
  if (!home) return std::nullopt;
  return fs::u8path(*home) / ".config" / "probnode.json";
}

Config Config::load() {
  auto p = path();
  return p ? load_from(*p) : Config{};
}

Config Config::load_from(const fs::path& file) {
  bool exists = false;
  json j = read_json(file, &exists);
  Config c;
  if (!j.is_object()) return c;
  if (auto it = j.find("api_key"); it != j.end() && it->is_string()) c.api_key = it->get<std::string>();
  if (auto it = j.find("base_url"); it != j.end() && it->is_string()) c.base_url = it->get<std::string>();
  return c;
}

fs::path Config::save() const {
  auto p = path();
  if (!p) throw Error(ErrorKind::Invalid, 0, "no home directory to save the config into");
  save_to(*p);
  return *p;
}

void Config::save_to(const fs::path& file) const {
  bool exists = false;
  json j = read_json(file, &exists);
  if (!j.is_object()) j = json::object();
  if (api_key) j["api_key"] = *api_key;
  if (base_url) j["base_url"] = *base_url;
  std::error_code ec;
  if (file.has_parent_path()) fs::create_directories(file.parent_path(), ec);
  {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) throw Error(ErrorKind::Io, 0, "cannot write " + file.string());
    out << j.dump(2);
    if (!out) throw Error(ErrorKind::Io, 0, "cannot write " + file.string());
  }
#ifndef _WIN32
  fs::permissions(file, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace,
                  ec);
#endif
}

}  // namespace probnode
