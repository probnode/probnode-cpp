// Copyright 2026 ProbNode. Licensed under the Apache License, Version 2.0.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace probnode {

/// SHA-256 (FIPS 180-4), bundled so the library needs no crypto dependency.
class Sha256 {
 public:
  Sha256() noexcept;
  /// Feeds bytes.
  void update(const void* data, std::size_t size) noexcept;
  void update(std::string_view data) noexcept { update(data.data(), data.size()); }
  /// The digest. The object must not be updated afterwards.
  std::array<std::uint8_t, 32> finish() noexcept;
  /// The digest as lowercase hex.
  std::string hex() noexcept;

 private:
  void block(const std::uint8_t* p) noexcept;
  std::array<std::uint32_t, 8> h_;
  std::array<std::uint8_t, 64> buf_{};
  std::size_t used_ = 0;
  std::uint64_t total_ = 0;
};

/// The sha256 of a string, lowercase hex.
std::string sha256_hex(std::string_view data);

/// The sha256 of a file, lowercase hex. Throws Error (Io) if it cannot be read.
std::string sha256_file(const std::filesystem::path& path);

}  // namespace probnode
