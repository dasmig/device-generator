"""Build resources/<tier>/ipv4.tsv and ipv6.tsv from the registries' statistics.

Source: the delegated-extended statistics of the five regional internet
registries (AFRINIC, APNIC, ARIN, LACNIC, RIPE NCC), the `allocated` and
`assigned` rows. Adjacent and overlapping IPv4 ranges of one country are
merged; IPv6 prefixes are collapsed.

Left out, because people's devices do not browse from them:

- the special-purpose registries (RFC 6890 and its updates);
- the old organisation blocks: registry rows from before 2000 that are a /9
  or larger (the US Department of Defense's /8s, Apple, Ford, Daimler, the UK
  Ministry of Defence, AMPRNet, ...) and the /8s IANA lists as LEGACY with an
  organisation as the designation;
- the data-centre ranges the large clouds publish: Amazon Web Services,
  Google Cloud, Microsoft Azure, Oracle Cloud and Cloudflare. IPv4 ranges are
  cut out of the blocks; an IPv6 prefix inside a cloud range is dropped.

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
import bisect
import csv
import ipaddress
import json
import re
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
IANA_V4 = "https://www.iana.org/assignments/ipv4-address-space/ipv4-address-space.csv"
CLOUDS = {
    "aws": "https://ip-ranges.amazonaws.com/ip-ranges.json",
    "gcp": "https://www.gstatic.com/ipranges/cloud.json",
    "oracle": "https://docs.oracle.com/en-us/iaas/tools/public_ip_ranges.json",
    "cloudflare-v4": "https://www.cloudflare.com/ips-v4",
    "cloudflare-v6": "https://www.cloudflare.com/ips-v6",
}
AZURE_PAGE = "https://www.microsoft.com/en-us/download/details.aspx?id=56519"

# Registry rows older than this and at least this large are organisation blocks.
LEGACY_BEFORE = "20000101"
LEGACY_SIZE = 1 << 23  # a /9

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
    path = RAW / name
    if refresh or not path.exists():
        RAW.mkdir(exist_ok=True)
        request = urllib.request.Request(url, headers={"User-Agent": "device-generator data build"})
        with urllib.request.urlopen(request, timeout=600) as response:
            path.write_bytes(response.read())
    return path


def countries(tier):
    with open(ROOT / "resources" / tier / "countries.tsv", encoding="utf-8", newline="") as f:
        rows = csv.DictReader((line for line in f if not line.startswith("#")), delimiter="\t")
        return {r["country_code"] for r in rows}


def iana_organisation_blocks(refresh):
    """The /8s IANA lists as LEGACY with an organisation, not a registry, as the designation."""
    path = fetch("iana-ipv4-address-space.csv", IANA_V4, refresh)
    out = []
    with open(path, encoding="utf-8", newline="") as f:
        for row in csv.DictReader(f):
            if row["Status [1]"].strip().upper() == "LEGACY" and not row["Designation"].startswith("Administered by"):
                first = int(row["Prefix"].split("/")[0])
                out.append(ipaddress.IPv4Network(f"{first}.0.0.0/8"))
    return out


def cloud_ranges(refresh):
    """IPv4 and IPv6 networks the large clouds publish as theirs."""
    v4, v6 = [], []

    def add(text):
        net = ipaddress.ip_network(text.strip(), strict=False)
        (v4 if net.version == 4 else v6).append(net)

    aws = json.loads(fetch("cloud-aws.json", CLOUDS["aws"], refresh).read_text(encoding="utf-8"))
    for p in aws["prefixes"]:
        add(p["ip_prefix"])
    for p in aws["ipv6_prefixes"]:
        add(p["ipv6_prefix"])
    gcp = json.loads(fetch("cloud-gcp.json", CLOUDS["gcp"], refresh).read_text(encoding="utf-8"))
    for p in gcp["prefixes"]:
        add(p.get("ipv4Prefix") or p.get("ipv6Prefix"))
    oracle = json.loads(fetch("cloud-oracle.json", CLOUDS["oracle"], refresh).read_text(encoding="utf-8"))
    for region in oracle["regions"]:
        for c in region["cidrs"]:
            add(c["cidr"])
    for key in ("cloudflare-v4", "cloudflare-v6"):
        for line in fetch(f"{key}.txt", CLOUDS[key], refresh).read_text(encoding="utf-8").split():
            add(line)
    azure_path = RAW / "cloud-azure.json"
    if refresh or not azure_path.exists():
        page = fetch("azure-download-page.html", AZURE_PAGE, True).read_text(encoding="utf-8", errors="replace")
        link = re.search(r"https://download\.microsoft\.com/download/[^\"]*ServiceTags_Public[^\"]*\.json", page)
        if link is None:
            raise SystemExit("no Azure service tags link on " + AZURE_PAGE)
        fetch("cloud-azure.json", link.group(0), True)
    azure = json.loads(azure_path.read_text(encoding="utf-8"))
    for value in azure["values"]:
        if value["name"] == "AzureCloud":  # every region's public ranges
            for prefix in value["properties"]["addressPrefixes"]:
                add(prefix)
    return ipaddress.collapse_addresses(v4), list(ipaddress.collapse_addresses(v6))


def merge(ranges):
    out = []
    for s, e in sorted(ranges):
        if out and s <= out[-1][1]:
            out[-1][1] = max(out[-1][1], e)
        else:
            out.append([s, e])
    return out


def subtract(ranges, holes):
    """Sorted, merged [s, e) ranges minus sorted, merged holes."""
    starts = [h[0] for h in holes]
    out = []
    for s, e in ranges:
        i = max(0, bisect.bisect_right(starts, s) - 1)
        cursor = s
        while i < len(holes) and holes[i][0] < e:
            hs, he = holes[i]
            if he > cursor:
                if hs > cursor:
                    out.append((cursor, min(hs, e)))
                cursor = max(cursor, he)
            i += 1
        if cursor < e:
            out.append((cursor, e))
    return out


def read_registries(refresh, organisation_v4):
    v4, v6 = {}, {}
    dropped = 0
    for name, url in REGISTRIES.items():
        path = fetch(f"delegated-{name}-extended-latest.txt", url, refresh)
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            p = line.split("|")
            if len(p) < 7 or p[1] in ("", "*") or p[6] not in ("allocated", "assigned"):
                continue
            code = p[1].upper()
            if p[2] == "ipv4":
                start, size = int(ipaddress.IPv4Address(p[3])), int(p[4])
                legacy = p[5] < LEGACY_BEFORE and size >= LEGACY_SIZE
                if legacy or any(ipaddress.IPv4Address(start) in n for n in organisation_v4):
                    dropped += size
                    continue
                v4.setdefault(code, []).append((start, start + size))
            elif p[2] == "ipv6":
                v6.setdefault(code, []).append(ipaddress.IPv6Network(f"{p[3]}/{p[4]}", strict=False))
    return v4, v6, dropped


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--refresh", action="store_true", help="download the sources again")
    parser.add_argument("--lite-blocks", type=int, default=64, help="blocks per country and family in lite")
    args = parser.parse_args()

    cloud4, cloud6 = cloud_ranges(args.refresh)
    holes4 = merge([(int(n.network_address), int(n.broadcast_address) + 1) for n in [*SPECIAL_V4, *cloud4]])
    raw4, raw6, legacy = read_registries(args.refresh, iana_organisation_blocks(args.refresh))
    blocks4, cut = {}, 0
    for code, ranges in raw4.items():
        merged = merge(ranges)
        kept = subtract(merged, holes4)
        cut += sum(e - s for s, e in merged) - sum(e - s for s, e in kept)
        blocks4[code] = kept
    excluded6 = [*SPECIAL_V6, *cloud6]
    blocks6 = {}
    for code, nets in raw6.items():
        collapsed = ipaddress.collapse_addresses(nets)
        blocks6[code] = [n for n in collapsed if not any(n.subnet_of(x) for x in excluded6 if x.prefixlen <= n.prefixlen)]
    print(f"left out: {legacy / 1e6:.0f} M legacy organisation addresses, "
          f"{cut / 1e6:.0f} M addresses in special-purpose and cloud ranges")

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
                  "APNIC, ARIN, LACNIC and RIPE NCC (allocated and assigned), without legacy organisation "
                  "blocks and the published cloud ranges. See doc/data.md.")
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
