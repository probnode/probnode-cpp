// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
//
// The saved login: the same file the `probnode` command line (Python) reads and writes. It is
// JSON, {"api_key": "...", "base_url": "..."}, at $PROBNODE_CONFIG or else
// ~/.config/probnode.json (on Windows %USERPROFILE%\.config\probnode.json).
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace probnode {

struct Config {
  std::optional<std::string> api_key;
  std::optional<std::string> base_url;

  /// $PROBNODE_CONFIG, else ~/.config/probnode.json; nullopt when no home directory is set.
  static std::optional<std::filesystem::path> path();
  /// The saved config, or an empty one when there is no file.
  static Config load();
  /// The config at `file`, or an empty one when it does not exist. Throws Error (Json) when it
  /// is not JSON.
  static Config load_from(const std::filesystem::path& file);
  /// Writes the config where path() says and returns that path. Other keys in an existing file
  /// are kept.
  std::filesystem::path save() const;
  /// Writes the config to `file`, creating its directory. Other keys in it are kept.
  void save_to(const std::filesystem::path& file) const;
};

}  // namespace probnode
