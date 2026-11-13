<p align="center">
  <a href="https://probnode.com"><img src="https://raw.githubusercontent.com/probnode/probnode-cpp/main/assets/probnode-logo.png" alt="ProbNode" width="96" height="96"></a>
</p>

<h1 align="center">probnode-cpp</h1>

<p align="center"><b>The official C++ client for ProbNode: Polymarket order books, trades and bars as sha256-checked files.</b></p>

<p align="center">
  <a href="https://github.com/probnode/probnode-cpp/actions/workflows/ci.yml"><img src="https://github.com/probnode/probnode-cpp/actions/workflows/ci.yml/badge.svg?branch=main" alt="CI"></a>
  <a href="https://github.com/probnode/probnode-cpp/blob/main/LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-blue.svg" alt="License: Apache-2.0"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17">
  <img src="https://img.shields.io/badge/CMake-3.18%2B-blue.svg" alt="CMake 3.18+">
</p>

## Overview

`probnode-cpp` is the official C++ client for the [ProbNode](https://probnode.com) API. ProbNode
records Polymarket at full depth: every change of every price level of every outcome token, every
trade, and every on-chain fill. The client lists, signs and downloads that data as parquet, csv.gz
and [Probe](https://github.com/probnode/probe) files, checks every file against its sha256, and
builds one market's UTC day of any market-day schema on request.

The same API has official clients for [Python](https://github.com/probnode/probnode-sdk) and
[Rust](https://github.com/probnode/probnode-rs), with the same methods.

## Features

- **Plain C++17.** One synchronous `probnode::Client`; public headers include only the standard
  library.
- **Every endpoint, typed.** Account, catalog, coverage, files, signed links, market-day exports
  and time series, as structs with `std::optional` for nullable fields.
- **Downloads you can trust.** Each file is written to a `.part`, checked against the sha256 the
  API listed, and only then renamed; a mismatch removes it and throws with kind `Checksum`.
- **Fifteen market-day schemas.** From the full book (`book-max`) to top of book, trades, fills,
  bars and daily market statistics, as parquet or csv.gz.
- **Few dependencies.** libcurl for HTTP and TLS, nlohmann/json kept private, and a bundled
  SHA-256: nothing links a crypto library for the checksum.
- **CMake native.** `find_package(probnode)` after install, or FetchContent; tested on Linux,
  macOS and Windows.
- **Thread-safe reads.** The `const` methods may be called from several threads at once.

## Installation

Requirements:

- A C++17 compiler: GCC 9+, Clang 10+, Apple Clang 12+, MSVC 2019+.
- CMake 3.18+.
- libcurl 7.68+ (with TLS: OpenSSL, Secure Transport or Schannel, as your libcurl was built):
  `apt install libcurl4-openssl-dev`, `dnf install libcurl-devel`, the macOS SDK, or
  `vcpkg install curl` on Windows (then pass vcpkg's toolchain file to CMake).
- [nlohmann/json](https://github.com/nlohmann/json) 3.11+, found if installed and fetched
  otherwise. It is private: no public header includes it.

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(probnode
  GIT_REPOSITORY https://github.com/probnode/probnode-cpp
  GIT_TAG v0.1.0)
FetchContent_MakeAvailable(probnode)

target_link_libraries(my_app PRIVATE probnode::probnode)
```

Each [release](https://github.com/probnode/probnode-cpp/releases) also carries a source archive
and its sha256, for a `URL` / `URL_HASH` pin or a package-manager port.

### find_package

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix /usr/local     # or any prefix on CMAKE_PREFIX_PATH
```

```cmake
find_package(probnode 0.1 REQUIRED)
target_link_libraries(my_app PRIVATE probnode::probnode)
```

## Quick start

```cpp
#include <probnode/client.hpp>

#include <iostream>

int main() {
  probnode::Client pn;  // PROBNODE_API_KEY, or the key `probnode login` saved

  // One market's UTC day of the full book: made on the first request, then found.
  probnode::ExportOptions opts;
  opts.schema = "book-max";
  opts.format = "parquet";
  auto day = pn.export_book("will-x-happen", "2026-11-12", opts);
  auto path = pn.download(day, "data/");  // sha256-checked; std::filesystem::path
  std::cout << day.rows << " rows in " << path << "\n";
}
```

The option structs are aggregates, so in C++20 syntax they can be written inline:

```cpp
auto day = pn.export_book("will-x-happen", "2026-11-12", {.schema = "book-max", .format = "parquet"});
```

## Usage

### The API, method by method

| method | call | key |
|---|---|---|
| `login(email, password)` | `POST /v1/auth/login`; the client uses the key from then on | no |
| `me()`, `usage()` | `GET /v1/me`, `GET /v1/data/usage` | yes |
| `datasets()` | `GET /v1/data/datasets`: every dataset, its formats, tier and hours held | no |
| `series(category, q)` | `GET /v1/catalog/series` | no |
| `markets(MarketQuery)` | `GET /v1/catalog/markets` (series, category, q, status, limit, offset) | no |
| `coverage(token_id, start, end)` | `GET /v1/catalog/coverage`: the recorded hours of a token | no |
| `samples()` | `GET /v1/samples`: free files with their links | no |
| `files(dataset, format, start, end, FilesQuery)` | `GET /v1/data/files`; `FilesQuery` sets granularity, markets and series | yes |
| `sign(paths)` | `POST /v1/data/urls`, batched by 100 | yes |
| `download(item, dest)` | sign if needed, fetch to `dest/<object_path>.part`, check the sha256, rename | yes* |
| `download_all(files, dest)` | sign in batches of 100, then download each | yes |
| `export_book(market, day, ExportOptions)` | `POST /v1/data/exports` with schema and format | yes |
| `timeseries(..)`, `timeseries_ndjson(..)` | `GET /v1/timeseries`: one market over a window, parquet bytes or NDJSON rows | yes |

\* A sample downloads without a key: its link is already signed.

`download` takes a `DataFile` from `files`, a `BookExport` from `export_book`, or a `Sample`.

### Hourly files

```cpp
probnode::Client pn;
probnode::FilesQuery q;
q.markets = {"will-x-happen"};
auto files = pn.files("trades", "parquet", "2026-11-12T00:00:00Z", "2026-11-12T06:00:00Z", q);
for (const auto& path : pn.download_all(files, "data/")) std::cout << path << "\n";

// One file per market and day instead of per hour.
probnode::FilesQuery days;
days.granularity = "market_day";
days.series = {"bitcoin-above"};
auto day_files = pn.files("trades", "csv", "2026-11-10T00:00:00Z", "2026-11-12T00:00:00Z", days);
```

### Market-day schemas

`export_book` builds any of the fifteen market-day schemas (`probnode::kMarketDaySchemas`):
`book-max` (the default), `book-10`, `book-top`, `book-top-1s`, `book-top-1m`, `trades`,
`trade-quotes`, `pair`, `fills`, `bars-1s`, `bars-1m`, `bars-1h`, `bars-1d`, `market-stats` and
`market-events`, as `parquet` or `csv` (gzip). The day must have ended (UTC). Each schema's
columns are described in the [schema reference](https://probnode.com/docs/schemas).

The `book_full` dataset's hourly files are Probe (`.evmin`); read them with the
[Probe](https://github.com/probnode/probe) reader.

### The key

`probnode::Client pn;` takes the key from `PROBNODE_API_KEY`, else from the file `probnode login`
saves: `~/.config/probnode.json` (on Windows `%USERPROFILE%\.config\probnode.json`), or the path in
`PROBNODE_CONFIG`. The base URL comes from `PROBNODE_BASE_URL`, the same file, or
`https://probnode.com`. `ClientOptions` sets them explicitly (and `from_env = false` reads
nothing). A key minted by `login` is shared with the command line through `probnode::Config`:

```cpp
probnode::Client pn;
std::string key = pn.login("you@example.com", "password");
probnode::Config c;
c.api_key = key;
c.base_url = pn.base_url();
c.save();
```

Get a key at [probnode.com/login](https://probnode.com/login).

### Errors

Every failure throws `probnode::Error`. `kind()` says what failed (`Api`, `NoApiKey`,
`Checksum`, `Transport`, `Io`, `Json`, `Invalid`); for an API refusal `status()` is the HTTP
status and `detail()` the API's own sentence, and `what()` reads
`402: your plan does not include book-max`. A keyed call made without a key throws `NoApiKey`
(status 401) and sends nothing.

### Threads

The `const` methods may be called from several threads at once; each call uses its own libcurl
handle. `login` changes the key and must not race with other calls.

### Examples and tests

```sh
cmake -S . -B build && cmake --build build -j
build/examples/probnode_datasets                       # no key needed
build/examples/probnode_export_book will-x-happen 2026-11-12 trades csv

ctest --test-dir build -R '^mock\.' --output-on-failure           # an in-process mock API
PROBNODE_LIVE_BASE_URL=https://probnode.com ctest --test-dir build -R '^live\.'   # keyless endpoints
```

The live tests are skipped unless `PROBNODE_LIVE_BASE_URL` is set.

## Documentation

- ProbNode documentation: [probnode.com/docs](https://probnode.com/docs)
- Schemas, columns and units: [probnode.com/docs/schemas](https://probnode.com/docs/schemas)
- API reference (OpenAPI 3.1): [probnode.com/openapi.json](https://probnode.com/openapi.json)
- The headers in [`include/probnode/`](https://github.com/probnode/probnode-cpp/tree/main/include/probnode)
  document every type and method.

## Versioning and compatibility

- The library follows [Semantic Versioning](https://semver.org); releases are tagged `vX.Y.Z`.
  Until 1.0, a minor version may change the API; every change is listed in the
  [CHANGELOG](https://github.com/probnode/probnode-cpp/blob/main/CHANGELOG.md).
- `find_package(probnode 0.1)` accepts any 0.1.x.
- C++17 and CMake 3.18 are the minimum; raising either is a minor-version change.
- Tested on Linux (GCC), macOS (Apple Clang) and Windows (MSVC).
- The client talks to the ProbNode API's `/v1`; fields the API adds later are ignored, so a newer
  API does not break an older client.

## Support

- Questions about your account, plan or the data: [hello@probnode.com](mailto:hello@probnode.com)
  or the [help centre](https://probnode.com/help).
- Bugs and feature requests for this client: [open an issue](https://github.com/probnode/probnode-cpp/issues/new/choose).
- Security reports: privately, as the [security policy](https://github.com/probnode/probnode-cpp/blob/main/SECURITY.md) says.

## Contributing

Pull requests are welcome; see [CONTRIBUTING.md](https://github.com/probnode/probnode-cpp/blob/main/CONTRIBUTING.md).
This project follows the [Contributor Covenant](https://github.com/probnode/probnode-cpp/blob/main/CODE_OF_CONDUCT.md).

## Other ProbNode libraries

| Repository | Language | What it is | Package |
|---|---|---|---|
| [probnode-sdk](https://github.com/probnode/probnode-sdk) | Python | The official Python client, the `probnode` command line and the MCP server | [`probnode-sdk`](https://pypi.org/project/probnode-sdk/), [`probnode-mcp`](https://pypi.org/project/probnode-mcp/) on PyPI |
| [probnode-rs](https://github.com/probnode/probnode-rs) | Rust | The official Rust client: async and blocking, typed models | `probnode` (crates.io release on the way) |
| [probe](https://github.com/probnode/probe) | Rust, Python | Probe, the lossless order-book file format: specification, library and CLI | [`probe-format`](https://crates.io/crates/probe-format), [`probe-cli`](https://crates.io/crates/probe-cli) on crates.io |

## License

Apache License 2.0. See [LICENSE](https://github.com/probnode/probnode-cpp/blob/main/LICENSE) and
[NOTICE](https://github.com/probnode/probnode-cpp/blob/main/NOTICE). Copyright 2026 ProbNode. The
license covers the code in this repository; the data you download is licensed under the terms of
your ProbNode plan.
