"""Build resources/<tier>/releases.tsv: when each OS and browser version came out.

Columns:
  product   os or browser key (ios, ipados, android, windows, macos, chrome,
            chrome-ios, edge, firefox, safari, samsung)
  version   the major version as the product names it (17, 11, 10.15, 126)
  released  first release date, ISO 8601
  full      a full version string for the user agent where one is used
            (Chrome's first stable build on that platform, iOS's last minor
            release, the Windows NT version); empty otherwise

Sources:
  ios, ipados, android, macos   endoflife.date API v1
  windows                       curated (four versions; dates from Microsoft)
  chrome, chrome-ios            Chromium Dash stable releases (Windows and iOS)
  firefox                       Mozilla product-details (major releases)
  edge                          Chrome's majors from 79 (Chromium-based Edge),
                                released a few days after Chrome; curated rule
  safari                        the iOS majors (Safari ships with iOS and the
                                same major for macOS)
  samsung                       curated from Samsung Internet's release notes
                                (approximate dates)

Only versions released by the end of the data are listed; dates before
2012 are dropped (the earliest reference date is 2021-01-01, and versions
older than that have no share by then), except Windows 7, which still had
users in 2021.

Usage: python scripts/prepare_releases.py [--refresh]
"""

import argparse
import datetime
import json
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "raw_data"

EOL = "https://endoflife.date/api/v1/products/{}/"
CHROMIUM = "https://chromiumdash.appspot.com/fetch_releases?channel=Stable&platform={}&num=1000"
FIREFOX = "https://product-details.mozilla.org/1.0/firefox.json"
EARLIEST = "2012-01-01"

# Windows versions with the NT version their user agents send.
WINDOWS = [("7", "2009-10-22", "6.1"), ("8.1", "2013-10-17", "6.3"), ("10", "2015-07-29", "10.0"),
           ("11", "2021-10-05", "10.0")]

# Samsung Internet majors: (version, approximate stable release, Chromium major).
SAMSUNG = [
    ("12.0", "2020-06-01", 79), ("13.0", "2020-11-01", 83), ("14.0", "2021-04-01", 87),
    ("15.0", "2021-08-01", 90), ("16.0", "2021-11-01", 92), ("17.0", "2022-04-01", 96),
    ("18.0", "2022-08-01", 99), ("19.0", "2022-11-01", 102), ("20.0", "2023-02-01", 106),
    ("21.0", "2023-05-01", 110), ("22.0", "2023-08-01", 111), ("23.0", "2023-11-01", 115),
    ("24.0", "2024-02-01", 117), ("25.0", "2024-05-01", 121), ("26.0", "2024-08-01", 122),
    ("27.0", "2024-11-01", 125), ("28.0", "2025-04-01", 130), ("29.0", "2025-09-01", 136),
]


def fetch(url, name, refresh):
    path = RAW / name
    if refresh or not path.exists():
        RAW.mkdir(exist_ok=True)
        with urllib.request.urlopen(url, timeout=300) as response:
            path.write_bytes(response.read())
    return json.loads(path.read_text(encoding="utf-8"))


def endoflife(product, refresh):
    data = fetch(EOL.format(product), f"endoflife-{product}.json", refresh)["result"]["releases"]
    rows = []
    for r in data:
        if not r.get("releaseDate"):
            continue
        latest = (r.get("latest") or {}).get("name") or ""
        rows.append((r["name"], r["releaseDate"], latest))
    return rows


def chromium(platform, refresh):
    """The first stable release of each milestone on a platform."""
    data = fetch(CHROMIUM.format(platform), f"chromium-{platform.lower()}.json", refresh)
    first = {}
    for r in data:
        when = datetime.datetime.fromtimestamp(r["time"] / 1000, datetime.UTC).date().isoformat()
        m = r["milestone"]
        if m not in first or when < first[m][0]:
            first[m] = (when, r["version"])
    return [(str(m), when, version) for m, (when, version) in first.items()]


def firefox(refresh):
    data = fetch(FIREFOX, "firefox.json", refresh)["releases"]
    rows = []
    for r in data.values():
        if r["category"] != "major":
            continue
        version = r["version"]
        major = version.split(".")[0]
        rows.append((major, r["date"], version))
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--refresh", action="store_true", help="download the sources again")
    args = parser.parse_args()

    rows = []
    for product in ("ios", "ipados", "android", "macos"):
        for version, released, latest in endoflife(product, args.refresh):
            rows.append((product, version, released, latest))
    rows += [("windows", v, d, nt) for v, d, nt in WINDOWS]
    chrome = chromium("Windows", args.refresh)
    rows += [("chrome", v, d, full) for v, d, full in chrome]
    rows += [("chrome-ios", v, d, full) for v, d, full in chromium("iOS", args.refresh)]
    rows += [("firefox", v, d, full) for v, d, full in firefox(args.refresh)]
    for v, d, _ in chrome:
        if int(v) >= 79:
            edge_day = datetime.date.fromisoformat(d) + datetime.timedelta(days=3)
            rows.append(("edge", v, edge_day.isoformat(), ""))
    rows += [("safari", v, d, latest) for p, v, d, latest in rows if p == "ios"]
    rows += [("samsung", v, d, f"{chromium_major}.0.0.0") for v, d, chromium_major in SAMSUNG]

    def sort_key(r):
        return (r[0], r[2], [int(x) if x.isdigit() else 0 for x in r[1].split(".")])

    rows = sorted({r for r in rows if r[2] >= EARLIEST or r[0] == "windows"}, key=sort_key)
    for tier in ("lite", "full"):
        out = ROOT / "resources" / tier / "releases.tsv"
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            f.write("# OS and browser major versions with their first release dates. Built by "
                    "scripts/prepare_releases.py from endoflife.date, Chromium Dash, Mozilla "
                    "product-details and curated rows. See doc/data.md.\n")
            f.write("product\tversion\treleased\tfull\n")
            f.writelines(f"{p}\t{v}\t{d}\t{full}\n" for p, v, d, full in rows)
    counts = {}
    for r in rows:
        counts[r[0]] = counts.get(r[0], 0) + 1
    print(f"{len(rows)} releases: " + ", ".join(f"{p} {n}" for p, n in sorted(counts.items())))
    print("newest:", max(r[2] for r in rows))


if __name__ == "__main__":
    main()
