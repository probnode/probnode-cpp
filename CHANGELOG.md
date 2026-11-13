# Changelog

probnode-cpp follows semantic versioning. Until 1.0, a minor version may change the API.

## 0.1.0 - unreleased

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
- CMake: `find_package(probnode)` after install, or FetchContent.
