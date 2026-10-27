"""Build resources/<tier>/countries.tsv: who is online in each country.

Columns:
  country_code    ISO 3166-1 alpha-2
  population      from country-generator (the side-by-side checkout)
  internet_share  share of the population using the internet, 0 to 1
                  (World Bank IT.NET.USER.ZS, latest year per country)
  ipv6_share      share of users whose connections prefer IPv6, 0 to 1
                  (APNIC Labs IPv6 measurement, the "preferred" column)

A country the World Bank has no value for gets the median of its UN
subregion, then of the world; a country APNIC does not measure gets 0
(the generator then draws IPv4 only). Both fallbacks are counted in the
report this script prints.

Usage: python scripts/prepare_countries.py [--refresh]
  Run from the repository root. Downloads go to raw_data/ and are reused
  unless --refresh is given. Needs ../country-generator.
"""

import argparse
import csv
import json
import re
import statistics
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "raw_data"
COUNTRY_GENERATOR = ROOT.parent / "country-generator"

WORLD_BANK = ("https://api.worldbank.org/v2/country/all/indicator/IT.NET.USER.ZS"
              "?format=json&per_page=400&mrnev=1")
APNIC = "https://stats.labs.apnic.net/ipv6"


def fetch(url, name, refresh):
    path = RAW / name
    if refresh or not path.exists():
        RAW.mkdir(exist_ok=True)
        with urllib.request.urlopen(url, timeout=300) as response:
            path.write_bytes(response.read())
    return path


def read_tsv(path):
    with open(path, encoding="utf-8", newline="") as f:
        return list(csv.DictReader((line for line in f if not line.startswith("#")), delimiter="\t"))


def internet_shares(refresh):
    data = json.loads(fetch(WORLD_BANK, "worldbank-it-net-user.json", refresh).read_text(encoding="utf-8"))
    out = {}
    for row in data[1]:
        if row["value"] is not None and row["countryiso3code"]:
            out[row["countryiso3code"]] = (row["value"] / 100.0, row["date"])
    return out


# One row of the table embedded in the APNIC page:
# ["<a href=\"/ipv6/DE\">DE</a>", "...", {v: 72.82, ...}, {v: 70.53, ...}, 14966005],
APNIC_ROW = re.compile(r'\["<a href=\\"/ipv6/([A-Z]{2})\\">[A-Z]{2}</a>",.*?\{v: ([0-9.]+),.*?\{v: ([0-9.]+),')


def ipv6_shares(refresh):
    text = fetch(APNIC, "apnic-ipv6.html", refresh).read_text(encoding="utf-8", errors="replace")
    return {m.group(1): float(m.group(3)) / 100.0 for m in APNIC_ROW.finditer(text)}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--refresh", action="store_true", help="download the sources again")
    args = parser.parse_args()

    if not (COUNTRY_GENERATOR / "resources").is_dir():
        sys.exit(f"needs {COUNTRY_GENERATOR} (clone country-generator next to this repository)")
    internet = internet_shares(args.refresh)
    ipv6 = ipv6_shares(args.refresh)
    print(f"World Bank: {len(internet)} economies; APNIC: {len(ipv6)} countries")

    for tier in ("lite", "full"):
        countries = read_tsv(COUNTRY_GENERATOR / "resources" / tier / "countries.tsv")
        by_subregion = {}
        for c in countries:
            if c["cca3"] in internet:
                by_subregion.setdefault(c["subregion"], []).append(internet[c["cca3"]][0])
        world = statistics.median(v for v, _ in internet.values())
        rows, subregion_fill, world_fill, no_v6 = [], 0, 0, 0
        for c in sorted(countries, key=lambda c: c["cca2"]):
            population = int(c["population"] or 0)
            if population <= 0:
                continue  # uninhabited territories cannot be drawn
            if c["cca3"] in internet:
                share = internet[c["cca3"]][0]
            elif c["subregion"] in by_subregion:
                share = statistics.median(by_subregion[c["subregion"]])
                subregion_fill += 1
            else:
                share = world
                world_fill += 1
            if c["cca2"] not in ipv6:
                no_v6 += 1
            rows.append((c["cca2"], population, min(max(share, 0.0), 1.0), ipv6.get(c["cca2"], 0.0)))

        out = ROOT / "resources" / tier / "countries.tsv"
        out.parent.mkdir(parents=True, exist_ok=True)
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            f.write("# Who is online in each country. Built by scripts/prepare_countries.py from "
                    "country-generator (population), the World Bank (IT.NET.USER.ZS) and APNIC Labs "
                    "(IPv6 preferred). See doc/data.md.\n")
            f.write("country_code\tpopulation\tinternet_share\tipv6_share\n")
            for code, population, share, v6 in rows:
                f.write(f"{code}\t{population}\t{share:.4f}\t{v6:.4f}\n")
        print(f"{tier}: {len(rows)} countries ({subregion_fill} internet shares from the subregion, "
              f"{world_fill} from the world, {no_v6} without an IPv6 measurement)")


if __name__ == "__main__":
    main()
