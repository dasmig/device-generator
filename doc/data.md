# Data and Method

The data in `resources/` and how device-generator uses it (rule 6 of the dasmig standard). Derived tables are rebuilt by the scripts in `scripts/`; curated tables are edited by hand and say so in their first line.

## Tiers

| Tier | Countries | IP data | Everything else |
| --- | --- | --- | --- |
| `lite` | 195 sovereign states (country-generator's `lite`) | the 64 largest IPv4 blocks and the 64 largest IPv6 prefixes per country: 9,301 and 7,156 rows | the same as `full` |
| `full` | 245 inhabited territories (country-generator's `full`) | every registered block: 145,071 IPv4 blocks and 69,734 IPv6 prefixes | |

`lite` is compiled in with `DASMIG_DEVICE_EMBED` (649 KB of source). The 64-block cap keeps the whole allocated IPv4 space for the median country; the most fragmented country keeps 30% of it. `full` loads in about 0.2 s.

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

Keyed by `country`, `platform`, `maker`; `weight` is relative, as of 2025-01-01, and the optional `trend` is the relative change per year before and after it (the weight at a date is `weight × (1 + trend)^years`). World rows for every platform with makers (Android phones and tablets, Windows, Linux and ChromeOS computers; Apple for its own platforms) and country rows for the Android phone makers of 31 markets, including Nigeria, Kenya, Ghana, Tanzania, Uganda and Bangladesh, where Tecno and Infinix are large. Trends: Huawei -25% a year outside China (+10% in China), Honor +15%, Tecno and Infinix +10%, Xiaomi +5%. Rough estimates rounded from public vendor share statistics (StatCounter and similar). Sharp, Sony, LG and itel are not modelled, so Japan is the least faithful market.

### `models.tsv`: device models, curated

Keyed by `key`: `maker`, `platform`, `model` (display name), `released` (first sale), `os_first` and `os_last` (the OS majors it runs; `os_last` empty means still updated), `width`, `height`, `pixel_ratio` (the screen in CSS pixels, portrait; 0 for a computer without its own screen) and `ua_model` (the model token its user agent sends where the browser sends one), `weight` (relative popularity). 180 models: 41 iPhones, 14 iPads, 15 Macs, 103 Android phones (from 2016) and 7 Android tablets.

- Release dates and OS ranges: from the makers' announcements and support pages, as known in mid-2026. Which iPhones get iOS 27 (September 2026) was not known when the data was written; models with an empty `os_last` are assumed to get it.
- Screens: CSS viewports of the default display settings; for iPhones and iPads they are exact, for Android phones they are rounded to the common 360, 393 and 412 point widths.
- Model tokens: Samsung, Google, Motorola, Huawei and OnePlus tokens are the models' build names. Xiaomi tokens are a mix of build codes (`M2101K6G`) and marketing names, as the phones themselves send. The Oppo, Vivo, Realme, Tecno, Infinix and Honor tokens follow the makers' code patterns (`CPH`, `V`, `RMX`, `TECNO`, `Infinix X`, `XXX-LX1`) but the mapping from a code to a model name is approximate.
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
| `opera` | from Chrome's: Opera N is built on Chromium N + 14 and ships about two weeks after it (a curated rule; the offset has drifted by a version at times) |
| `yandex` | from Chrome's: Yandex Browser YY.M is built on the Chromium released that month and ships about three weeks after it (a curated rule) |

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

### `bundled.tsv`: the browser an OS version ships with, curated

Keyed by `os`, `os_version`, `browser`: `min_browser`. On that OS version the browser is at least that major: macOS 10.15 to 15 ship Safari 13 to 18, macOS 26 and 27 ship Safari 26 and 27, so a Mac on macOS 26 never shows Safari 18. From Apple's release notes.

### `browser_shares.tsv`: browsers, curated

Keyed by `country`, `os`, `browser`: `weight`, as of 2025-01-01, and an optional yearly `trend` as in `maker_shares.tsv` (Edge on Windows +6%, Firefox -8% on desktops, Samsung Internet -3%, Chrome on iOS +3%, Opera -3%). World rows per OS and country rows for South Korean Android, German Windows, and Russia, Belarus and Kazakhstan (Yandex Browser). Rounded from StatCounter's browser shares by platform. Samsung Internet is only drawn on Samsung devices (about a sixth of their users with the shipped weights). A requested browser narrows the country and platform draws to those that have it. UC Browser, QQ Browser and the makers' own Chinese browsers are left out, so China is the least faithful.

### `ua_templates.tsv`: user-agent formats, curated

Keyed by `browser`, `os`, `min_browser`, `min_os`: `template`. The row used is the one with the highest (`min_browser`, `min_os`) the device reaches. Placeholders: `{os}` (the OS version), `{os_}` (with underscores), `{os_full}` (the Windows NT version), `{model}` (the model token, or `K`), `{v}` (the browser version), `{full}` (its full version, or `{v}.0.0.0`), `{mobile}` (`Mobile ` on phones), `{form}` (`Mobile` or `Tablet`). The formats follow the browsers' documentation and observed strings:

- Chrome: full versions before 101, minor zeros from 101 (user-agent reduction phase 4), `Android 10; K` from 110 (phase 6). The desktop OS tokens are the frozen ones (`Windows NT 10.0` for 10 and 11, `Intel Mac OS X 10_15_7`, `CrOS x86_64 14541.0.0`) at every version, a simplification before 2022.
- Edge: Chrome's format with `Edg/`, `EdgA/` and `EdgiOS/` and minor zeros at every version (a simplification before 2022).
- Samsung Internet: the model token with a `SAMSUNG` prefix before version 20, the reduced form from 20 (approximate version).
- Safari on iOS: `Version/` is the iOS version; from iOS 26 the OS token is frozen at `18_6`. Every iOS browser has the same frozen token. iPad Safari sends the Mac desktop string from iPadOS 13, and the iPad string before.
- Firefox: `rv:` and `Firefox/` with the major, `Intel Mac OS X 10.15` on Macs, `FxiOS/` on iOS.
- Opera: Chrome's desktop format with its Chromium version and `OPR/`. Yandex Browser: its Chromium build and `YaBrowser/` with `Yowser/2.5` on desktops, and the unreduced `arm_64; Android {os}; {model}` form with `SA/3` on Android, as its real strings have.
- Chrome on iPad sends the iPad string at every version; whether recent versions ask for the desktop site, as Safari does, is not verified.

### `screens.tsv`: screens without a model, curated

Keyed by `country`, `os`, `width`, `height`, `pixel_ratio`: `weight`. Used for Windows, Linux and ChromeOS computers, Macs without their own screen (Mac mini) and phones whose model has none. Rounded from StatCounter's resolution shares; the CSS sizes include common scaling (1920 × 1080 at 125% is 1536 × 864 at ratio 1.25).

### `ipv4.tsv` and `ipv6.tsv`: registered blocks, derived

Keyed by `country_code` and `start` (`size` in addresses) or `prefix` (`length`). Built by `scripts/prepare_ip.py` from the delegated-extended statistics of AFRINIC, APNIC, ARIN, LACNIC and RIPE NCC (`allocated` and `assigned` rows): IPv4 ranges of a country merged, IPv6 prefixes collapsed. Left out, because people's devices do not browse from them:

- the IANA special-purpose ranges (RFC 6890 and updates);
- old organisation blocks, 369 million addresses: registry rows from before 2000 that are a `/9` or larger (the US Department of Defense's `/8`s, Apple, Ford, Daimler, the UK Ministry of Defence, AMPRNet and others) and the `/8`s IANA's IPv4 registry lists as LEGACY with an organisation as the designation;
- the data-centre ranges Amazon Web Services, Google Cloud, Microsoft Azure (the `AzureCloud` service tag), Oracle Cloud and Cloudflare publish: cut out of the IPv4 blocks (with the special-purpose ranges, 182 million addresses); an IPv6 prefix inside one is dropped.

Without them the United States has 1,134 million addresses instead of 1,606 million. Large companies' and smaller hosting providers' blocks remain, as do VPN and mobile carrier pools. The country of a block is where it is registered, which is not always where it is used (a multinational's block, a VPN).

### `reserved.tsv`: documentation ranges

Keyed by `version` and `prefix`: `length`. RFC 5737 (`192.0.2.0/24`, `198.51.100.0/24`, `203.0.113.0/24`) and RFC 3849 (`2001:db8::/32`); the only addresses a `safe` device gets.

### Client hints

Chrome and Edge from version 96, outside iOS, get the three low-entropy client hints they send with every request. `Sec-CH-UA` lists a "GREASE" brand, `Chromium` and the browser (`Google Chrome`, `Microsoft Edge`) with the major version; from 103 the three are ordered by one of six permutations chosen by the major version, and from 105 the GREASE brand is `Not?A?Brand` with two characters and a version (8, 99 or 24) chosen by it, as in Chromium's `GenerateBrandVersionList`. Versions 96 to 102 and 104 use `" Not A;Brand";v="99"`, 103 `".Not/A)Brand";v="99"`. The tests check the headers of Chrome 103, 104, 105, 106, 108, 110, 120 and 124 against real ones. Samsung Internet, Opera and Yandex Browser send client hints too, but their brand lists are not modelled, so they get none, and neither does Chrome before 96.

## Generation Model

Each step draws from its own stream split from the call seed, so a step fixed by the request leaves the others unchanged.

1. **Country** (`"country"`): the request's, else by internet users.
2. **Platform** (`"platform"`): the request's, else by the country's platform shares, among the platforms the requested kind and browser allow.
3. **Maker and model** (`"model"`): a maker by the country's shares among those with a model out by the date (Samsung only, when Samsung Internet is asked for), then a model of that maker by popularity and age. Platforms without models (Windows, Linux, ChromeOS) have a maker and no model.
4. **OS version** (`"os-version"`, `"os-minor"`): among the versions released by the date that the model runs, by `version_shares.tsv` or the adoption curve. iOS and iPadOS add a minor version: the newest one out by the date (about one every 45 days, up to the last known), or one to a few older with falling chance.
5. **Browser** (`"browser"`): by the OS's shares in the country at the date (with trends), among browsers with a version that runs on that OS version by the date.
6. **Browser version** (`"browser-version"`): among the versions released by the date that run on the OS and are no older than the one it ships with, by the adoption curve. Safari on iOS is the iOS version; Safari on macOS adds a minor version like iOS.
7. **User agent and client hints**: rendered from the template; client hints for Chrome and Edge; `Accept-Language` from the requested locale (`pt-BR,pt;q=0.9,en;q=0.8`).
8. **Screen** (`"screen"`): the model's, else by `screens.tsv`.
9. **IP** (`"ip-version"`, `"ip"`): IPv6 with the country's IPv6 share (or as requested), then a block by size and an address inside it, avoiding `.0` and `.255` in IPv4; IPv6 keeps the prefix and draws the rest. `safe` draws from `reserved.tsv` instead.

## Checks

- The tests draw thousands of devices at six dates from 2021 to 2031 and check that every part agrees with the others and with the date, and that every address is inside the country's blocks.
- `scripts/validate_user_agents.py` parses 80,000 generated user agents with ua-parser (uap-core). On the shipped data it reads back the browser family, its major version and the OS family for 100% of them, (Opera and Yandex Browser included), and the device maker for 87% of the strings that carry a model token. The misses are ua-parser's: it does not map Xiaomi's code tokens or Vivo's `V` codes to a brand, and files Realme's `RMX` codes under Oppo.

## Rebuilding the Data

```bash
python scripts/prepare_countries.py   # needs ../country-generator
python scripts/prepare_ip.py          # after prepare_countries.py
python scripts/prepare_releases.py
make embed                            # regenerate dasmig/devicegen_data.hpp
```

Downloads are kept in `raw_data/` (not in the repository) and reused; `--refresh` downloads them again. A rebuild changes the known answers.

## Known Limitations

- **Trends are simple.** Maker and browser shares change by a constant yearly rate from their 2025 values; platform and IPv6 shares do not change with the date.
- **Missing makers and browsers.** Sharp, Sony, LG and itel phones, Amazon tablets, UC Browser and the Chinese makers' browsers are not modelled.
- **Client hints** are modelled for Chrome and Edge only (see above).
- **Approximations.** Samsung Internet's release months, the Android and iOS browser requirements, Edge's release days and the pre-2022 desktop user-agent tokens are approximate.
- **Not modelled.** High-entropy client hints (`Sec-CH-UA-Full-Version-List` and the like, sent only when a site asks), in-app browsers and WebViews, bots and crawlers, smart TVs, consoles and wearables, the network operator of an address, and hardware identifiers (IMEI, MAC), which are left out on purpose.
