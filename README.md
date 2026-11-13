<p align="center">
  <a href="https://probnode.com"><img src="assets/probnode-logo.png" alt="ProbNode" width="96" height="96"></a>
</p>

<h1 align="center">probnode-cpp</h1>

<p align="center">
  <a href="https://github.com/probnode/probnode-cpp/actions/workflows/ci.yml"><img src="https://github.com/probnode/probnode-cpp/actions/workflows/ci.yml/badge.svg?branch=main" alt="CI"></a>
  <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg" alt="C++17">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-Apache--2.0-blue.svg" alt="Apache-2.0"></a>
</p>

The official C++ client for [ProbNode](https://probnode.com): prediction-market order books at
full depth, trades, top-of-book grids, bars and market statistics, delivered as parquet, csv.gz
and [Probe](https://github.com/probnode/probe) files with a sha256 for every file.

The same API has an official [Python client](https://github.com/probnode/probnode-sdk) and an
official [Rust client](https://github.com/probnode/probnode-rs).

## Requirements

- A C++17 compiler: GCC 9+, Clang 10+, Apple Clang 12+, MSVC 2019+.
- CMake 3.18+.
- libcurl 7.68+ (with TLS: OpenSSL, Secure Transport or Schannel, as your libcurl was built).
- [nlohmann/json](https://github.com/nlohmann/json) 3.11+, found if installed and fetched
  otherwise. It is private: no public header includes it.

The sha256 check is bundled; nothing links a crypto library for it.

## Install

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(probnode
  GIT_REPOSITORY https://github.com/probnode/probnode-cpp
  GIT_TAG v0.1.0)
FetchContent_MakeAvailable(probnode)

target_link_libraries(my_app PRIVATE probnode::probnode)
```

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

libcurl comes from your system: `apt install libcurl4-openssl-dev`, `dnf install libcurl-devel`,
the macOS SDK, or `vcpkg install curl` on Windows (then pass vcpkg's toolchain file to CMake).

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

The option structs are aggregates, so with C++20 they can be written inline:

```cpp
auto day = pn.export_book("will-x-happen", "2026-11-12", {.schema = "book-max", .format = "parquet"});
```

## The API, method by method

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

`download` takes a `DataFile` from `files`, a `BookExport` from `export_book`, or a `Sample`. The
file is written to `<object_path>.part` and renamed only when complete and its sha256 is the one
the API listed; on a mismatch the partial file is removed and `probnode::Error` is thrown with
kind `Checksum`.

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
columns are described in the [schema reference](https://probnode.com/docs).

The `book_full` dataset's hourly files are Probe (`.evmin`); read them with the
[Probe](https://github.com/probnode/probe) reader.

## The key

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

## Errors

Every failure throws `probnode::Error`. `kind()` says what failed (`Api`, `NoApiKey`,
`Checksum`, `Transport`, `Io`, `Json`, `Invalid`); for an API refusal `status()` is the HTTP
status and `detail()` the API's own sentence, and `what()` reads
`402: your plan does not include book-max`. A keyed call made without a key throws `NoApiKey`
(status 401) and sends nothing.

## Threads

The `const` methods may be called from several threads at once; each call uses its own libcurl
handle. `login` changes the key and must not race with other calls.

## Examples

```sh
cmake -S . -B build && cmake --build build -j
build/examples/probnode_datasets                       # no key needed
build/examples/probnode_export_book will-x-happen 2026-11-12 trades csv
```

## Tests

```sh
ctest --test-dir build -R '^mock\.' --output-on-failure           # an in-process mock API
PROBNODE_LIVE_BASE_URL=https://probnode.com ctest --test-dir build -R '^live\.'   # keyless endpoints
```

The live tests are skipped unless `PROBNODE_LIVE_BASE_URL` is set.

## License

Apache-2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE).
