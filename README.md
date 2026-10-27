# Device Generator for C++

> **Requires C++23** (e.g., `-std=c++23` for GCC/Clang, `/std:c++latest` for MSVC).

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/dasmig/device-generator/main/LICENSE.MIT)
[![CI](https://github.com/dasmig/device-generator/actions/workflows/ci.yml/badge.svg)](https://github.com/dasmig/device-generator/actions/workflows/ci.yml)
[![GitHub Releases](https://img.shields.io/github/release/dasmig/device-generator.svg)](https://github.com/dasmig/device-generator/releases)
[![C++23](https://img.shields.io/badge/standard-C%2B%2B23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Header-only](https://img.shields.io/badge/type-header--only-green.svg)](https://github.com/dasmig/device-generator#integration)
[![Documentation](https://img.shields.io/badge/docs-GitHub%20Pages-blue.svg)](https://dasmig.github.io/device-generator/)

**[API Reference](https://dasmig.github.io/device-generator/)** · **[Usage Guide](doc/usage.md)** · **[Data and Method](doc/data.md)** · **[Releases](https://github.com/dasmig/device-generator/releases)**

Devices, browsers, user agents and IP addresses that fit together: a phone model that was on sale at the date, running an OS version it supports, a browser that runs on that OS at a version released by then, the user-agent string that exact combination sends, its screen, and an address registered in the person's country.

```
Chrome 114 on Android 13 (Samsung Galaxy A14), 178.46.39.249
Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Mobile Safari/537.36
```

## Features

- **Everything Agrees**. The model came out before the reference date, the OS version is one the model runs, the browser runs on that OS version (Chrome 110 and later need Windows 10; Safari only on Apple systems; every iOS browser is WebKit), and versions are those released by the date, with newer ones spreading as they really do.

- **Real User Agents**. Strings follow each browser's format at that version: Chrome's reduced user agent from version 110 on Android (`Android 10; K`), the minor-version zeros from 101, Apple's frozen `18_6` token from iOS 26, `CriOS`, `FxiOS` and `EdgiOS` on iOS, iPad Safari asking for the desktop site. Checked against ua-parser, which reads back the browser, its major version and the OS for every generated string.

- **Real Addresses, or Safe Ones**. IPv4 and IPv6 addresses come from the blocks the five regional registries list for the country, weighted by size, never from special-purpose ranges. `safe` gives documentation addresses only (RFC 5737, RFC 3849), so test data reaches no one.

- **Any Date**. A `reference_date` from 2021 on picks the devices, OS and browser versions of that day; the library never reads the clock.

- **Ask for What You Need**. A `device_request` can fix the country, the platform (user-generator's keys), the kind, the browser, the IP family and the locale (for `Accept-Language`). What is left out is drawn, and fixing one part leaves the others' draws unchanged.

- **Deterministic Seeding**. Per-call `get_device(request, seed)` for reproducible results, and `device::seed()` for replaying a previous generation with the same request and data.

- **Batch Generation**. Generate thousands of results from one seed, on several threads with the same output for any thread count, optionally unique by a key such as the IP, and stream them as JSON Lines or CSV (see the usage guide).

- **Identical Everywhere**. A seed gives the same result on every compiler, standard library and CPU (GCC, Clang, MSVC, Emscripten; x86-64 and ARM64), checked by known-answer tests.

- **Extensible Data**. Fifteen TSV tables (models, releases, shares, user-agent templates, IP blocks, ...) that you can extend with layers (`extend()`), checked before they are applied.

- **JSON and Replay**. `to_json()` on every result, and a `signature()` recording the seed and the data it came from.

- **Compiled-in Data**. Define `DASMIG_DEVICE_EMBED` to build the `lite` tier into the program, for WebAssembly and other builds without files.

- **Multi-Instance Support**. Construct independent `dvg` instances with their own data and random engine.

## Integration

[`devicegen.hpp`](https://github.com/dasmig/device-generator/blob/main/dasmig/devicegen.hpp) is the library [released here](https://github.com/dasmig/device-generator/releases). Keep [`core.hpp`](https://github.com/dasmig/device-generator/blob/main/dasmig/core.hpp) and the [`core/`](https://github.com/dasmig/device-generator/tree/main/dasmig/core) directory ([dasmig-core](https://github.com/dasmig/dasmig-core)) in the same directory. Add

```cpp
#include <dasmig/devicegen.hpp>
```

to the files you want to use it in, and supply the [`resources`](https://github.com/dasmig/device-generator/tree/main/resources) folder (or define `DASMIG_DEVICE_EMBED`).

## Usage

```cpp
#include <dasmig/devicegen.hpp>
#include <iostream>

auto& gen = dasmig::dvg::instance(); // loads the lite tier

auto d = gen.get_device(dasmig::device_request{});
std::cout << d << '\n';            // Chrome 114 on Android 13 (Samsung Galaxy A14), 178.46.39.249
std::cout << d.user_agent << '\n'; // the User-Agent header
std::cout << d.to_json() << '\n';  // every field, with the seed

dasmig::device_request request;
request.country = "BR";
request.platform = "ios-phone";                        // user-generator's platform keys
request.locale = "pt-BR";                              // Accept-Language: pt-BR,pt;q=0.9,en;q=0.8
request.reference_date = std::chrono::year{2026} / 10 / 1;
request.safe = true;                                   // a documentation address
auto iphone = gen.get_device(request, 42);

// The same seed gives the same result everywhere.
auto again = gen.get_device(request, iphone.seed());
```

See the [usage guide](doc/usage.md) for everything else.

## Building

```bash
make            # example
make test       # tests
make bench      # benchmarks, results in build/bench.tsv
make test-embed # tests with the lite tier compiled in
make coverage   # code coverage
make lint       # clang-tidy
make docs       # API docs
make embed      # regenerate dasmig/devicegen_data.hpp from resources/lite
```

## Compiler Support

Tested with:
- Clang 18+ (`-std=c++23`)
- GCC 14+ (`-std=c++23`)
- MSVC 19.38+ (`/std:c++latest`)
- Emscripten 3.1.74

## Dependencies

| Dependency | Version | Bundled | Purpose |
|-----------|---------|---------|---------|
| [dasmig-core](https://github.com/dasmig/dasmig-core) | 1.2 | Yes (`core.hpp`, `core/`) | Platform-independent random engine and distributions, data layers, JSON |
| [Catch2](https://github.com/catchorg/Catch2) | 3.x | Yes (amalgamated) | Unit testing |

## Related Libraries

| Library | Description |
|---------|-------------|
| [address-generator](https://github.com/dasmig/address-generator) | Postal addresses with real streets by place |
| [biodata-generator](https://github.com/dasmig/biodata-generator) | Procedural human physical characteristics |
| [birth-generator](https://github.com/dasmig/birth-generator) | Demographically plausible birthdays |
| [city-generator](https://github.com/dasmig/city-generator) | Weighted city selection by population |
| [country-generator](https://github.com/dasmig/country-generator) | Weighted country selection by population |
| [email-generator](https://github.com/dasmig/email-generator) | Email addresses in each country's style |
| [entity-generator](https://github.com/dasmig/entity-generator) | ECS-based entity generation |
| [interest-generator](https://github.com/dasmig/interest-generator) | Religion, hobbies and favourite media by country, age and sex |
| [name-generator](https://github.com/dasmig/name-generator) | Culturally appropriate full names |
| [nickname-generator](https://github.com/dasmig/nickname-generator) | Gamer-style nicknames |
| [occupation-generator](https://github.com/dasmig/occupation-generator) | Employment status, education and occupation by country, age and sex |
| [persona-generator](https://github.com/dasmig/persona-generator) | Complete characters composed from the other generators |
| [phone-generator](https://github.com/dasmig/phone-generator) | Phone numbers valid in each country's numbering plan |
| [relationship-generator](https://github.com/dasmig/relationship-generator) | Families: parents, siblings, partners and children with names by convention |
| [trait-generator](https://github.com/dasmig/trait-generator) | Personality traits with conflicts, synergies and Big Five axes |
| [user-generator](https://github.com/dasmig/user-generator) | Synthetic user records: account, username, contacts |

## License

This library is released under the [MIT License](https://opensource.org/licenses/MIT). The data's sources are listed in [LICENSE_DATA.txt](LICENSE_DATA.txt) and [doc/data.md](doc/data.md).

```
MIT License

Copyright (c) 2026 Diego Dasso Migotto
```
