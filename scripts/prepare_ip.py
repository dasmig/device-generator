"""Build resources/<tier>/ipv4.tsv and ipv6.tsv from the registries' statistics.

Source: the delegated-extended statistics of the five regional internet
registries (AFRINIC, APNIC, ARIN, LACNIC, RIPE NCC), the `allocated` and
`assigned` rows. Adjacent and overlapping IPv4 ranges of one country are
merged; IPv6 prefixes are collapsed. Ranges inside the special-purpose
registries (RFC 6890 and its updates) are dropped, so they are never drawn.

  ipv4.tsv  country_code, start (dotted quad), size (addresses)
  ipv6.tsv  country_code, prefix (compressed), length

`full` keeps every block of the countries in its countries.tsv; `lite` keeps
the K largest blocks of each of its countries (ties by address), which is
enough for the blocks to be drawn by size without the long tail.

The country of a block is where it is registered, not where it is used.

Usage: python scripts/prepare_ip.py [--refresh] [--lite-blocks K]
  Run from the repository root after prepare_countries.py. Downloads go to
  raw_data/ and are reused unless --refresh is given.
"""

import argparse
import csv
import ipaddress
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "raw_data"

REGISTRIES = {
    "afrinic": "https://ftp.afrinic.net/pub/stats/afrinic/delegated-afrinic-extended-latest",
    "apnic": "https://ftp.apnic.net/stats/apnic/delegated-apnic-extended-latest",
    "arin": "https://ftp.arin.net/pub/stats/arin/delegated-arin-extended-latest",
    "lacnic": "https://ftp.lacnic.net/pub/stats/lacnic/delegated-lacnic-extended-latest",
    "ripencc": "https://ftp.ripe.net/pub/stats/ripencc/delegated-ripencc-extended-latest",
}

# IANA special-purpose address registries (RFC 6890 and updates) and the
# multicast and reserved blocks.
SPECIAL_V4 = [ipaddress.IPv4Network(n) for n in (
    "0.0.0.0/8", "10.0.0.0/8", "100.64.0.0/10", "127.0.0.0/8", "169.254.0.0/16", "172.16.0.0/12",
    "192.0.0.0/24", "192.0.2.0/24", "192.31.196.0/24", "192.52.193.0/24", "192.88.99.0/24",
    "192.168.0.0/16", "192.175.48.0/24", "198.18.0.0/15", "198.51.100.0/24", "203.0.113.0/24",
    "224.0.0.0/4", "240.0.0.0/4")]
SPECIAL_V6 = [ipaddress.IPv6Network(n) for n in (
    "::/8", "64:ff9b::/96", "64:ff9b:1::/48", "100::/64", "2001::/23", "2001:db8::/32", "2002::/16",
    "3fff::/20", "5f00::/16", "fc00::/7", "fe80::/10", "ff00::/8")]


def fetch(name, url, refresh):
    path = RAW / f"delegated-{name}-extended-latest.txt"
    if refresh or not path.exists():
        RAW.mkdir(exist_ok=True)
        with urllib.request.urlopen(url, timeout=600) as response:
            path.write_bytes(response.read())
    return path


def countries(tier):
    with open(ROOT / "resources" / tier / "countries.tsv", encoding="utf-8", newline="") as f:
        rows = csv.DictReader((line for line in f if not line.startswith("#")), delimiter="\t")
        return {r["country_code"] for r in rows}


def subtract_v4(start, end, holes):
    """[start, end) minus the special ranges, as a list of [s, e) pieces."""
    pieces = [(start, end)]
    for net in holes:
        hs, he = int(net.network_address), int(net.broadcast_address) + 1
        next_pieces = []
        for s, e in pieces:
            if he <= s or hs >= e:
                next_pieces.append((s, e))
                continue
            if s < hs:
                next_pieces.append((s, hs))
            if he < e:
                next_pieces.append((he, e))
        pieces = next_pieces
    return pieces


def read_registries(refresh):
    v4, v6 = {}, {}
    for name, url in REGISTRIES.items():
        for line in fetch(name, url, refresh).read_text(encoding="utf-8", errors="replace").splitlines():
            p = line.split("|")
            if len(p) < 7 or p[1] in ("", "*") or p[6] not in ("allocated", "assigned"):
                continue
            code = p[1].upper()
            if p[2] == "ipv4":
                start = int(ipaddress.IPv4Address(p[3]))
                for s, e in subtract_v4(start, start + int(p[4]), SPECIAL_V4):
                    v4.setdefault(code, []).append((s, e))
            elif p[2] == "ipv6":
                net = ipaddress.IPv6Network(f"{p[3]}/{p[4]}", strict=False)
                if not any(net.overlaps(h) for h in SPECIAL_V6):
                    v6.setdefault(code, []).append(net)
    return v4, v6


def merge(ranges):
    out = []
    for s, e in sorted(ranges):
        if out and s <= out[-1][1]:
            out[-1][1] = max(out[-1][1], e)
        else:
            out.append([s, e])
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--refresh", action="store_true", help="download the statistics again")
    parser.add_argument("--lite-blocks", type=int, default=64, help="blocks per country and family in lite")
    args = parser.parse_args()

    raw4, raw6 = read_registries(args.refresh)
    blocks4 = {c: merge(r) for c, r in raw4.items()}
    blocks6 = {c: sorted(ipaddress.collapse_addresses(n)) for c, n in raw6.items()}

    for tier in ("lite", "full"):
        wanted = countries(tier)
        cap = args.lite_blocks if tier == "lite" else None
        out4, out6, covered = [], [], []
        for code in sorted(wanted):
            b4 = blocks4.get(code, [])
            if cap is not None and len(b4) > cap:
                keep = sorted(b4, key=lambda r: (-(r[1] - r[0]), r[0]))[:cap]
                covered.append(sum(e - s for s, e in keep) / sum(e - s for s, e in b4))
                b4 = sorted(keep)
            out4 += [(code, str(ipaddress.IPv4Address(s)), e - s) for s, e in b4]
            b6 = blocks6.get(code, [])
            if cap is not None and len(b6) > cap:
                b6 = sorted(sorted(b6, key=lambda n: (n.prefixlen, n.network_address))[:cap])
            out6 += [(code, str(n.network_address), n.prefixlen) for n in b6]

        directory = ROOT / "resources" / tier
        source = ("Built by scripts/prepare_ip.py from the delegated-extended statistics of AFRINIC, "
                  "APNIC, ARIN, LACNIC and RIPE NCC (allocated and assigned). See doc/data.md.")
        with open(directory / "ipv4.tsv", "w", encoding="utf-8", newline="\n") as f:
            f.write(f"# IPv4 blocks registered to each country, merged. {source}\n")
            f.write("country_code\tstart\tsize\n")
            f.writelines(f"{c}\t{s}\t{n}\n" for c, s, n in out4)
        with open(directory / "ipv6.tsv", "w", encoding="utf-8", newline="\n") as f:
            f.write(f"# IPv6 prefixes registered to each country, collapsed. {source}\n")
            f.write("country_code\tprefix\tlength\n")
            f.writelines(f"{c}\t{p}\t{n}\n" for c, p, n in out6)
        missing4 = sorted(wanted - {c for c, _, _ in out4})
        note = ""
        if covered:
            covered.sort()
            note = f"; capped countries keep {covered[0]:.0%} to {covered[-1]:.0%} of their space"
        print(f"{tier}: {len(out4)} IPv4 blocks, {len(out6)} IPv6 prefixes{note}; "
              f"no IPv4 block: {', '.join(missing4) or 'none'}")


if __name__ == "__main__":
    main()
