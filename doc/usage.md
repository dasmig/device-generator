# Usage Guide

device-generator draws the device a person uses to reach a service: platform, maker and model, OS version, browser and version, the `User-Agent` and `Accept-Language` headers, the screen and an IP address. Every part agrees with the others and with the reference date. This guide covers everything beyond the README.

- [Quick Start](#quick-start)
- [Installation](#installation)
- [Loading Resources](#loading-resources)
- [The Request](#the-request)
- [The Result](#the-result)
- [Dates and Versions](#dates-and-versions)
- [IP Addresses and safe](#ip-addresses-and-safe)
- [With user-generator](#with-user-generator)
- [Seeding and Deterministic Generation](#seeding-and-deterministic-generation)
- [Extending Data](#extending-data)
- [Replay and JSON](#replay-and-json)
- [Compiled-in Data](#compiled-in-data)
- [Thread Safety](#thread-safety)
- [Batch Generation](#batch-generation)
- [Error Reference](#error-reference)

## Quick Start

```cpp
#include <dasmig/devicegen.hpp>
#include <iostream>

int main()
{
    auto& gen = dasmig::dvg::instance();
    auto d = gen.get_device(dasmig::device_request{});
    std::cout << d << '\n' << d.user_agent << '\n';
}
```

```
Chrome 114 on Android 13 (Samsung Galaxy A14), 178.46.39.249
Mozilla/5.0 (Linux; Android 10; K) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/114.0.0.0 Mobile Safari/537.36
```

## Installation

Copy `dasmig/devicegen.hpp`, `dasmig/core.hpp` and `dasmig/core/` into your include path, and either the `resources/` folder or `dasmig/devicegen_data.hpp` (see [Compiled-in Data](#compiled-in-data)). Build with `-std=c++23` (GCC 14+, Clang 18+) or `/std:c++latest` (MSVC 19.38+).

## Loading Resources

`dvg::instance()` loads the `lite` tier on first use, from the compiled-in copy or from a `resources/` folder found next to the program (`resources/`, `../resources`, `device-generator/resources`, `../device-generator/resources`). Independent instances start empty:

```cpp
dasmig::dvg gen;
gen.load(dasmig::dataset::full);              // 245 territories, every registered IP block
gen.load(std::filesystem::path{"my/data"});   // a directory with countries.tsv and platforms.tsv
```

`lite` covers the 195 sovereign states with the 64 largest IP blocks per country; `full` covers 245 territories with all 143,000 IPv4 blocks and 70,000 IPv6 prefixes and loads in about 0.2 s. Both have the same models, versions and shares. `countries()` lists the codes a request can name, `platforms()` the platform keys.

## The Request

| Field | Default | Meaning |
| --- | --- | --- |
| `country` | drawn by internet users | ISO 3166-1 alpha-2 code, any case |
| `platform` | drawn by the country's shares | `android-phone`, `ios-phone`, `windows-desktop`, `macos-desktop`, `ipad`, `android-tablet`, `linux-desktop`, `chromeos-desktop`, `other` (user-generator's keys) |
| `kind` | any | `device_kind::phone`, `tablet`, `desktop` or `other`; narrows the platform draw |
| `browser` | drawn by the OS's shares | `chrome`, `safari`, `edge`, `firefox`, `samsung`, `opera`, `yandex`; a drawn country and platform are ones that have it |
| `locale` | empty | BCP 47 tag for `Accept-Language`; empty leaves the header empty |
| `reference_date` | 2023-07-01 | the day the device is seen; from 2021-01-01 |
| `ip` | `ip_version::any` | `any` (IPv6 with the country's IPv6 share), `v4`, `v6` |
| `safe` | false | documentation addresses only |
| `weighted` | true | false draws uniformly among what is possible |

```cpp
dasmig::device_request request;
request.kind = dasmig::device_kind::desktop;
request.browser = "firefox";
request.ip = dasmig::ip_version::v6;
auto d = gen.get_device(request);   // Firefox on Windows, macOS or Linux, with an IPv6 address
```

Fixing a field never changes the other fields' draws: `get_device({.country = d.country_code}, d.seed())` gives the same device as `get_device({}, d.seed())`. A platform fixes the kind and the OS; `platform = "other"` gives a device with no browser and no user agent (the platform exists so that every user-generator platform can be passed in).

## The Result

| Member | Example |
| --- | --- |
| `country_code` | `BR` |
| `platform`, `platform_label`, `kind` | `ios-phone`, `iPhone`, `phone` |
| `maker`, `model` | `Apple`, `iPhone 15`; Windows, Linux and ChromeOS computers have a maker and no model |
| `os`, `os_version` | `iOS`, `17.5`; Android, Windows and macOS versions are majors (`14`, `11`, `10.15`); Linux and ChromeOS have none |
| `browser`, `browser_version`, `engine` | `chrome`, `126`, `WebKit` (every iOS browser is WebKit) |
| `user_agent` | the `User-Agent` header |
| `accept_language` | `pt-BR,pt;q=0.9,en;q=0.8` |
| `sec_ch_ua`, `sec_ch_ua_mobile`, `sec_ch_ua_platform` | `"Chromium";v="124", "Google Chrome";v="124", "Not-A.Brand";v="99"`, `?0`, `"Windows"`: the client hints Chrome and Edge 96 and later send outside iOS; empty otherwise |
| `screen` | `{width, height, pixel_ratio}` in CSS pixels, portrait for phones and tablets |
| `ip`, `ip_version`, `reserved` | `2804:38a:96bb:5ece:67b6:b52a:7139:f2d1`, `6`, `false` |

`to_string()` (and `operator<<`) gives a one-line summary; `device::browser_name("samsung")` gives `Samsung Internet`.

## Dates and Versions

The reference date decides which models exist and which OS and browser versions are out, and moves maker and browser shares along their trends (shares in the data are those of 2025; Huawei phones outside China are about three times as common in 2021). Versions spread after release and fade once the next version is out, so soon after a release most devices are still on the previous one; a device stays on the last version it can run (an iPhone 8 on iOS 16, Chrome 109 on Windows 7). The user agent follows the browser's format at that version: Chrome on Android sends its model before Chrome 110 and `Android 10; K` from 110, and iOS 26 and later send the frozen `iPhone OS 18_6`.

```cpp
dasmig::device_request then;
then.reference_date = std::chrono::year{2021} / 6 / 1;   // Chrome 91, iOS 14, Windows 7 still around
dasmig::device_request now;
now.reference_date = std::chrono::year{2026} / 10 / 1;   // iPhone 17, iOS 26 and 27
```

Dates before 2021-01-01 throw `std::invalid_argument`. A date after the newest release in the data (2026-10-27 in this version) is treated as that date, so the newest versions keep their shares instead of fading.

## IP Addresses and safe

Addresses are drawn from the blocks the regional registries list for the country, weighted by size; special-purpose ranges are never drawn, and IPv4 addresses avoid `.0` and `.255`. IPv6 addresses keep the registered prefix and draw the rest. A block's country is where it is registered, not necessarily where it is used.

`safe = true` draws only from the documentation ranges (RFC 5737: `192.0.2.0/24`, `198.51.100.0/24`, `203.0.113.0/24`; RFC 3849: `2001:db8::/32`) and sets `reserved`. A country with no registered block (Kosovo in the shipped data) needs `safe`.

## With user-generator

The platform keys are user-generator's, so a user's device can be drawn to match the account:

```cpp
auto u = dasmig::usg::instance().get_user({});
dasmig::device_request request;
request.country = u.country_code;
request.platform = u.account.platform;
request.locale = u.account.locale;
request.reference_date = /* the date of u.account.last_login, or of u.account.created */;
auto d = dasmig::dvg::instance().get_device(request, u.seed());
```

user-generator itself does not include device-generator yet; an opt-in `device` section is planned.

## Seeding and Deterministic Generation

`get_device(request, seed)` is a pure function of the request, the seed and the data, and is `const`; `get_device(request)` draws a seed from the instance's engine. `device::seed()` gives the seed back. The same request, seed and data give byte-identical output on every compiler, standard library and CPU, checked by the known-answer tests.

## Extending Data

Each of the sixteen tables (`dasmig::device_table`) takes layers:

```cpp
// A new phone model.
gen.extend_from_string(dasmig::device_table::models,
    "key\tmaker\tplatform\tmodel\treleased\tos_first\tos_last\twidth\theight\tpixel_ratio\tua_model\tweight\n"
    "my-phone\tSamsung\tandroid-phone\tGalaxy Test\t2024-02-01\t14\t\t384\t832\t2.8125\tSM-T999B\t5\n",
    "my-models.tsv");

// Portugal on Linux only.
gen.extend_from_string(dasmig::device_table::platform_shares, "country\tkey\tweight\nPT\tlinux-desktop\t100\n",
                       "shares.tsv");

// Every table at once from a directory, or one file by its name.
gen.extend(std::filesystem::path{"my-layers/"});
```

In the default `override` mode a row replaces the loaded row with the same key and new keys are added; `layer_mode::add` refuses existing keys; `layer_mode::replace` replaces the table; a `disabled` column set to 1 removes a key. A layer is checked before it is applied, and a bad one leaves the data unchanged. `layers(table)` lists the layers in effect; `load()` drops them all. The columns are described in [doc/data.md](data.md).

## Replay and JSON

`to_json()` writes every field and a `signature` with the seed and `data_hash()`, the hash of the loaded tables and layers:

```json
{"country_code":"PK","platform":"ios-phone","platform_label":"iPhone","kind":"phone","maker":"Apple","model":"iPhone 15 Plus","os":"iOS","os_version":"26.7","browser":"safari","browser_version":"26.7","engine":"WebKit","user_agent":"Mozilla/5.0 (iPhone; CPU iPhone OS 18_6 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.7 Mobile/15E148 Safari/604.1","accept_language":"pt-BR,pt;q=0.9,en;q=0.8","client_hints":{"sec_ch_ua":"","sec_ch_ua_mobile":"","sec_ch_ua_platform":""},"screen":{"width":430,"height":932,"pixel_ratio":3},"ip":"125.209.109.2","ip_version":4,"reserved":false,"signature":{"seed":"0000000000000007","data":"…"}}
```

`to_json(core::json_writer&)` writes into a larger document.

## Compiled-in Data

Define `DASMIG_DEVICE_EMBED` before including the header (or pass `-DDASMIG_DEVICE_EMBED`) to build the `lite` tier into the program from `dasmig/devicegen_data.hpp` (649 KB of source). `load(dataset::lite)` then needs no files, and its output equals the files'. `make embed` regenerates the header after the data changes.

## Thread Safety

Instances are independent. The seeded `get_device(request, seed)` is `const` and safe to call from several threads on one instance; the unseeded overload, `load()` and `extend()` change the instance and need external synchronisation.

## Batch Generation

Every getter has batch forms: `get_device_batch` returns a `std::vector`, and `for_each_device` streams the items to a sink in index order without holding the batch.

```cpp
auto& gen = dasmig::dvg::instance();
const auto request = dasmig::device_request{};

// 1,000 results from one batch seed, on four threads.
auto items = gen.get_device_batch(request, {.count = 1000, .threads = 4}, 42);

// Item 1 replays on its own: the batch seed and the index give its seed.
auto again = gen.get_device(request, dasmig::core::item_seed(42, 1));

// Unique addresses.
auto unique = gen.get_device_batch(request,
    {.count = 1000, .unique = dasmig::core::unique_rule::by<dasmig::device>([](const dasmig::device& d) { return d.ip; })},
    42);

// A million results to a file as JSON Lines, never all in memory.
std::ofstream out{"devices.jsonl"};
dasmig::core::jsonl_writer lines{out};
gen.for_each_device(request, {.count = 1'000'000, .threads = 0}, 42, lines);
```

- **Item seeds.** Item `i` is exactly `get_device(request, core::item_seed(batch_seed, first + i))`. A range can be generated directly (`first = 1000` starts at item 1000), two batches that follow each other equal one, and the output is the same for any `threads` (0 means the hardware count).
- **Options.** `core::batch_options` has `count`, `first`, `threads` and `unique`. The request describes one item; the options describe the batch.
- **Uniqueness.** `.unique = core::unique_rule::by<device>(key)` regenerates an item whose key already appeared, with a deterministic retry seed, and throws `std::runtime_error` naming the item when the attempts run out.
- **Sinks.** `core::jsonl_writer` writes one `to_json()` per line, `core::csv_writer` flattens each result's JSON into dotted columns (`screen.width`). Any callable `(std::uint64_t index, device&)` works.

About 9 µs a device on one thread (110,000 a second, `lite`, clang `-O2`).

## Error Reference

| Exception | When |
|-----------|------|
| `std::runtime_error` | A generation call before data is loaded |
| `std::invalid_argument` | A reference date before 2021-01-01 or not a valid date; an unknown country or platform; a kind that contradicts the platform; a browser that does not run on the platform or the OS version by the date; a country without registered addresses and no `safe`; a `locale` that is not a BCP 47 tag (a language of 2 to 8 letters, then subtags of 1 to 8 letters or digits, separated by `-` or `_`) |
| `dasmig::core::tsv_error` | A malformed table or layer (message gives file, line and column); the data is unchanged |
| `std::invalid_argument` from `extend(path)` | A file whose name is not one of the tables |
