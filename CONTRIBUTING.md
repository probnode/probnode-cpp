# Contributing to probnode-cpp

Thank you for helping. Bug reports, fixes, build fixes for other platforms and improvements to
the documentation are all welcome.

## Ground rules

- **The API is the contract.** The client mirrors the ProbNode HTTP API; a method that needs a
  change on the service side starts as an issue, not a pull request.
- **The same surface as the other clients.** The Python and Rust clients have the same methods;
  a new method here should exist, or be planned, there too.
- **Never trust a download.** Every file is written to a `.part`, checked against its sha256 and
  only then renamed. A change to `download` keeps that, with a test.
- **The key stays with the API.** It is never sent to a signed-link host or across a redirect.
- **C++17, and no new public dependencies.** nlohmann/json stays private; public headers include
  only the standard library.

## Development

You need CMake 3.18+, a C++17 compiler and libcurl with its headers.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPROBNODE_WERROR=ON
cmake --build build -j
ctest --test-dir build -R '^mock\.' --output-on-failure
```

The mock tests serve a fake API in-process and need no network. The live tests call the public,
keyless endpoints and run only when `PROBNODE_LIVE_BASE_URL` is set:

```sh
PROBNODE_LIVE_BASE_URL=https://probnode.com ctest --test-dir build -R '^live\.' --output-on-failure
```

A new test case is a `TEST(name)` in `tests/mock_tests.cpp` and its name in the list in
`tests/CMakeLists.txt` (a check fails if the two disagree).

CI builds and tests on Linux, macOS and Windows.

## Pull requests

- One topic per pull request, with a test that fails without the change.
- Add a line to `CHANGELOG.md` under `[Unreleased]`.

## Code of conduct

This project follows the [Contributor Covenant](CODE_OF_CONDUCT.md). By participating you agree
to uphold it.

## License

By contributing you agree that your contributions are licensed under the Apache License 2.0, as
the project is (section 5 of the license).
