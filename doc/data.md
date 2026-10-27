# Data and Method

The data in `resources/` and how device-generator uses it (rule 6 of the dasmig standard). Derived tables are rebuilt by the scripts in `scripts/`; curated tables are edited by hand and say so in their first line.

## Tiers

| Tier | Countries | IP data | Everything else |
| --- | --- | --- | --- |
| `lite` | 195 sovereign states (country-generator's `lite`) | the 64 largest IPv4 blocks and the 64 largest IPv6 prefixes per country: 9,302 and 7,156 rows | the same as `full` |
| `full` | 245 inhabited territories (country-generator's `full`) | every registered block: 143,403 IPv4 blocks and 69,743 IPv6 prefixes | |

`lite` is compiled in with `DASMIG_DEVICE_EMBED` (630 KB of source). The 64-block cap keeps the whole allocated IPv4 space for the median country; the most fragmented country keeps 30% of it. `full` loads in about 0.2 s.

## Tables

Every table is a TSV file with a `#` comment on its first line, keyed by the columns given, layered and extensible (`extend()`; a `disabled` column removes a row). "Country `*`" is the default for countries without rows of their own; a country's rows replace the default for that platform or OS.

### `countries.tsv`: who is online, derived

Keyed by `country_code`. Built by `scripts/prepare_countries.py`.

| Column | Source and method | Reliability |
| --- | --- | --- |
| `population` | country-generator's `population` | as country-generator |
| `internet_share` | World Bank `IT.NET.USER.ZS` (individuals using the internet, % of population), latest year per country (mostly 2023 or 2024), divided by 100. Without a value: the median of the UN subregion (2 countries in `lite`, 31 in `full`), else of the world (2 in `full`) | good for large countries; small territories are subregion estimates |
| `ipv6_share` | APNIC Labs IPv6 measurement, the "IPv6 preferred" share per country, parsed from the table on `stats.labs.apnic.net/ipv6`. Without a measurement: 0 (IPv4 only) | good; it is today's share, applied to every reference date |

A country is drawn by `population × internet_share`, among those with IPv4 blocks.

### `platforms.tsv` and `platform_shares.tsv`: platforms, curated

The same keys, labels, global weights and per-country percentages as user-generator's tables, so a user's platform can be passed in as `platform` and the shares agree. `platforms.tsv` is keyed by `key` (`label`, `device` = phone, tablet, desktop or other, `os`, `weight`); `platform_shares.tsv` by `country` and `key` (`weight`, a percentage). A country's listed platforms get its percentages; the others share the rest of 100 by global weight. Authored estimates rounded from public usage statistics.

### `maker_shares.tsv`: makers, curated

Keyed by `country`, `platform`, `maker`; `weight` is relative. World rows for every platform with makers (Android phones and tablets, Windows, Linux and ChromeOS computers; Apple for its own platforms) and country rows for the Android phone makers of 26 large markets. Rough estimates rounded from public vendor share statistics (StatCounter and similar). Makers without models in `models.tsv` are not listed, which leaves out Tecno, Infinix, Honor, Sharp, Sony and LG; markets where they are large (Nigeria, Japan) are the least faithful.

### `models.tsv`: device models, curated

Keyed by `key`: `maker`, `platform`, `model` (display name), `released` (first sale), `os_first` and `os_last` (the OS majors it runs; `os_last` empty means still updated), `width`, `height`, `pixel_ratio` (the screen in CSS pixels, portrait; 0 for a computer without its own screen) and `ua_model` (the model token its user agent sends where the browser sends one), `weight` (relative popularity). 142 models: 41 iPhones, 14 iPads, 15 Macs, 65 Android phones and 7 Android tablets.

- Release dates and OS ranges: from the makers' announcements and support pages, as known in mid-2026. Which iPhones get iOS 27 (September 2026) was not known when the data was written; models with an empty `os_last` are assumed to get it.
- Screens: CSS viewports of the default display settings; for iPhones and iPads they are exact, for Android phones they are rounded to the common 360, 393 and 412 point widths.
- Model tokens: Samsung, Google, Motorola, Huawei and OnePlus tokens are the models' build names. Xiaomi tokens are a mix of build codes (`M2101K6G`) and marketing names, as the phones themselves send. The Oppo, Vivo and Realme tokens follow the makers' code patterns (`CPH`, `V`, `RMX`) but the mapping from a code to a model name is approximate.
- Popularity: rough relative sales within a maker.

### `releases.tsv`: OS and browser versions, derived

Keyed by `product` and `version`: `released` (first stable release) and `full` (a full version string for the user agent: Chrome's first stable build, iOS's last minor release, the Windows NT version). Built by `scripts/prepare_releases.py`.

| Product | Source |
| --- | --- |
| `ios`, `ipados`, `android`, `macos` | endoflife.date API v1 (Android 4.4W and 12L are left out: phones never ran them) |
| `windows` | curated: 7, 8.1, 10 and 11 with Microsoft's release dates and NT versions |
| `chrome`, `chrome-ios` | Chromium Dash, the first stable release of each milestone on Windows and on iOS |
| `firefox` | Mozilla product-details, the first release of each major |
| `edge` | Chrome's majors from 79 (the first Chromium-based Edge), three days after Chrome's: Edge follows Chrome's schedule; the exact days differ by a few |
| `safari` | the iOS majors and dates: Safari ships with each iOS and the same major comes to macOS |
| `samsung` | curated: Samsung Internet 12 to 29 with approximate release months and their Chromium majors, from the release notes |

The newest release in the data is its "as of" date (2026-10-27): a reference date after it is seen as that date.

### `adoption.tsv`: how versions and models spread, curated

Keyed by `product` (an OS or browser key, or `model:<platform>`): `ramp_days`, `half_life_days`.

- A **model**'s weight grows linearly for `ramp_days` after its release, then halves every `half_life_days`, times its popularity.
- An **OS or browser version** grows the same way and starts halving once the next version the device could update to is out (a version it can run, released by the date). The newest version a device can run keeps its users, so an iPhone 8 stays on iOS 16 and Chrome on Windows 7 stays on 109.

Values were chosen so that the drawn shares match published version shares at mid-2023: iOS 16 at 81% (StatCounter about 77 to 80%), the current Chrome at about two thirds, Android 13, 12, 11 and 10 at 33, 26, 23 and 16%.

### `version_shares.tsv`: explicit version shares, curated

Keyed by `product`, `version`, `date`: `share`. A product listed here uses these shares (interpolated linearly between dates, held after the last) instead of `adoption.tsv`; a version has no share before its release. Only Windows is listed, from StatCounter's desktop Windows version shares at the start of each year from 2021 to 2026, because Windows 7 and 10 lasted far longer than a decay curve allows.

### `requirements.tsv`: the oldest OS a browser runs on, curated

Keyed by `browser`, `os`, `from_version`: `min_os`. From that browser version on, the OS must be at least `min_os` (Chrome and Edge 110 need Windows 10; Firefox 116 needs Windows 10 and macOS 10.15; Chrome 120 needs Android 8; Safari 17 needs macOS 12). From the vendors' support notices; the Android and iOS rows are approximate.

### `browser_shares.tsv`: browsers, curated

Keyed by `country`, `os`, `browser`: `weight`. World rows per OS and country rows for South Korean Android and German Windows. Rounded from StatCounter's browser shares by platform. Samsung Internet is only drawn on Samsung devices (about a sixth of their users with the shipped weights). Opera, Yandex, UC Browser, QQ Browser and the makers' own Chinese browsers are left out, so Russia and China are the least faithful.

### `ua_templates.tsv`: user-agent formats, curated

Keyed by `browser`, `os`, `min_browser`, `min_os`: `template`. The row used is the one with the highest (`min_browser`, `min_os`) the device reaches. Placeholders: `{os}` (the OS version), `{os_}` (with underscores), `{os_full}` (the Windows NT version), `{model}` (the model token, or `K`), `{v}` (the browser version), `{full}` (its full version, or `{v}.0.0.0`), `{mobile}` (`Mobile ` on phones), `{form}` (`Mobile` or `Tablet`). The formats follow the browsers' documentation and observed strings:

- Chrome: full versions before 101, minor zeros from 101 (user-agent reduction phase 4), `Android 10; K` from 110 (phase 6). The desktop OS tokens are the frozen ones (`Windows NT 10.0` for 10 and 11, `Intel Mac OS X 10_15_7`, `CrOS x86_64 14541.0.0`) at every version, a simplification before 2022.
- Edge: Chrome's format with `Edg/`, `EdgA/` and `EdgiOS/` and minor zeros at every version (a simplification before 2022).
- Samsung Internet: the model token with a `SAMSUNG` prefix before version 20, the reduced form from 20 (approximate version).
- Safari on iOS: `Version/` is the iOS version; from iOS 26 the OS token is frozen at `18_6`. Every iOS browser has the same frozen token. iPad Safari sends the Mac desktop string.
- Firefox: `rv:` and `Firefox/` with the major, `Intel Mac OS X 10.15` on Macs, `FxiOS/` on iOS.

### `screens.tsv`: screens without a model, curated

Keyed by `country`, `os`, `width`, `height`, `pixel_ratio`: `weight`. Used for Windows, Linux and ChromeOS computers, Macs without their own screen (Mac mini) and phones whose model has none. Rounded from StatCounter's resolution shares; the CSS sizes include common scaling (1920 × 1080 at 125% is 1536 × 864 at ratio 1.25).

### `ipv4.tsv` and `ipv6.tsv`: registered blocks, derived

Keyed by `country_code` and `start` (`size` in addresses) or `prefix` (`length`). Built by `scripts/prepare_ip.py` from the delegated-extended statistics of AFRINIC, APNIC, ARIN, LACNIC and RIPE NCC (`allocated` and `assigned` rows): IPv4 ranges of a country merged, IPv6 prefixes collapsed, and the IANA special-purpose ranges (RFC 6890 and updates) removed. The country of a block is where it is registered, which is not always where it is used (a multinational's block, a VPN).

### `reserved.tsv`: documentation ranges

Keyed by `version` and `prefix`: `length`. RFC 5737 (`192.0.2.0/24`, `198.51.100.0/24`, `203.0.113.0/24`) and RFC 3849 (`2001:db8::/32`); the only addresses a `safe` device gets.

## Generation Model

Each step draws from its own stream split from the call seed, so a step fixed by the request leaves the others unchanged.

1. **Country** (`"country"`): the request's, else by internet users.
2. **Platform** (`"platform"`): the request's, else by the country's platform shares, among the platforms the requested kind and browser allow.
3. **Maker and model** (`"model"`): a maker by the country's shares among those with a model out by the date (Samsung only, when Samsung Internet is asked for), then a model of that maker by popularity and age. Platforms without models (Windows, Linux, ChromeOS) have a maker and no model.
4. **OS version** (`"os-version"`, `"os-minor"`): among the versions released by the date that the model runs, by `version_shares.tsv` or the adoption curve. iOS and iPadOS add a minor version: the newest one out by the date (about one every 45 days, up to the last known), or one to a few older with falling chance.
5. **Browser** (`"browser"`): by the OS's shares in the country, among browsers with a version that runs on that OS version by the date.
6. **Browser version** (`"browser-version"`): among the versions released by the date that run on the OS, by the adoption curve. Safari on iOS is the iOS version; Safari on macOS adds a minor version like iOS.
7. **User agent**: rendered from the template; `Accept-Language` from the requested locale (`pt-BR,pt;q=0.9,en;q=0.8`).
8. **Screen** (`"screen"`): the model's, else by `screens.tsv`.
9. **IP** (`"ip-version"`, `"ip"`): IPv6 with the country's IPv6 share (or as requested), then a block by size and an address inside it, avoiding `.0` and `.255` in IPv4; IPv6 keeps the prefix and draws the rest. `safe` draws from `reserved.tsv` instead.

## Checks

- The tests draw thousands of devices at six dates from 2021 to 2031 and check that every part agrees with the others and with the date, and that every address is inside the country's blocks.
- `scripts/validate_user_agents.py` parses 80,000 generated user agents with ua-parser (uap-core). On the shipped data it reads back the browser family, its major version and the OS family for 100% of them, and the device maker for 84% of the strings that carry a model token. The misses are ua-parser's: it does not map Xiaomi's code tokens or Vivo's `V` codes to a brand, and files Realme's `RMX` codes under Oppo.

## Rebuilding the Data

```bash
python scripts/prepare_countries.py   # needs ../country-generator
python scripts/prepare_ip.py          # after prepare_countries.py
python scripts/prepare_releases.py
make embed                            # regenerate dasmig/devicegen_data.hpp
```

Downloads are kept in `raw_data/` (not in the repository) and reused; `--refresh` downloads them again. A rebuild changes the known answers.

## Known Limitations

- **Today's shares at every date.** The platform, maker, browser and IPv6 shares are recent; a 2021 device is drawn with 2025 to 2026 shares (only versions and models change with the date).
- **Old devices are scarce.** The models start in 2015 to 2019, so Android 9 and older are rarer than they were (2% at mid-2023 against about 8%).
- **Missing makers and browsers.** Tecno, Infinix, Honor, Sharp, Sony and LG phones, Amazon tablets, Opera, Yandex, UC Browser and the Chinese makers' browsers are not modelled.
- **Approximations.** Samsung Internet's release months, the Android and iOS browser requirements, Edge's release days and the pre-2022 desktop user-agent tokens are approximate.
- **Not modelled.** Client hints (`Sec-CH-UA-*`, planned for 1.1), in-app browsers and WebViews, bots and crawlers, smart TVs, consoles and wearables, the network operator of an address, and hardware identifiers (IMEI, MAC), which are left out on purpose.
