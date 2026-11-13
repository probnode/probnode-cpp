# Changelog

All notable changes to probnode-cpp are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the library follows
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Until 1.0, a minor version may change
the API.

## [Unreleased]

## [0.1.0] - 2026-11-13

First release of the official C++ client.

### Added

- `probnode::Client` (synchronous, libcurl): `login`, `me`, `usage`; `datasets`, `series`,
  `markets`, `coverage`, `samples`; `files` (hourly or `market_day`, narrowed by market and
  series); `sign` (batched by 100); `download` and `download_all` (a `.part` file, the sha256
  checked, then renamed); `export_book` (any of the fifteen market-day schemas, parquet or
  csv.gz); `timeseries` (parquet bytes) and `timeseries_ndjson`.
- Typed structs for every answer; nullable fields are `std::optional`.
- `probnode::Error` carries the kind, the HTTP status and the API's sentence.
- The key from `ClientOptions`, `PROBNODE_API_KEY`, or the file `probnode login` saves
  (`~/.config/probnode.json`, shared with the Python command line).
- A bundled SHA-256, `probnode::Sha256` and `probnode::sha256_file`.
- CMake: `find_package(probnode)` after install, or FetchContent. Tested on Linux, macOS and
  Windows.

[Unreleased]: https://github.com/probnode/probnode-cpp/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/probnode/probnode-cpp/releases/tag/v0.1.0
