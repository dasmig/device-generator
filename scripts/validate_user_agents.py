"""Check generated user agents against ua-parser, the parser behind most analytics.

Builds scripts/sample_devices.cpp, draws devices at several reference dates
and parses each user agent with ua-parser (uap-core's rules). Reports how
often the browser family and major version, the OS family and the device
brand it reads agree with what the generator drew, and lists the worst
disagreements. Not a test: ua-parser has its own blind spots (frozen tokens
read as "Android 10", iPad Safari read as a Mac), which are reported, not
fixed.

Usage: python scripts/validate_user_agents.py [--count N] [--cxx clang++]
  Run from the repository root. Needs `pip install "ua-parser[regex]"`.
"""

import argparse
import collections
import json
import subprocess
import sys
from pathlib import Path

from ua_parser import parse

ROOT = Path(__file__).resolve().parent.parent
DATES = ["2021-06-01", "2023-07-01", "2025-03-01", "2026-10-01"]

BROWSER_FAMILIES = {
    "chrome": {"Chrome", "Chrome Mobile", "Chrome Mobile iOS", "Chrome Mobile WebView"},
    "safari": {"Safari", "Mobile Safari"},
    "edge": {"Edge", "Edge Mobile"},
    "firefox": {"Firefox", "Firefox Mobile", "Firefox iOS"},
    "samsung": {"Samsung Internet"},
    "opera": {"Opera"},
    "yandex": {"Yandex Browser"},
}
OS_FAMILIES = {
    "Android": {"Android"},
    "iOS": {"iOS"},
    "iPadOS": {"iOS", "Mac OS X"},  # iPad Safari asks for the desktop site
    "Windows": {"Windows"},
    "macOS": {"Mac OS X"},
    "Linux": {"Linux", "Ubuntu"},
    "ChromeOS": {"Chrome OS"},
}


def sample(count, cxx):
    exe = ROOT / "build" / "sample_devices"
    exe.parent.mkdir(exist_ok=True)
    subprocess.run([cxx, "-std=c++23", "-O2", "-I", str(ROOT), "-o", str(exe),
                    str(ROOT / "scripts" / "sample_devices.cpp")], check=True)
    out = subprocess.run([str(exe), str(count), *DATES], check=True, capture_output=True, text=True,
                         cwd=ROOT).stdout
    return [json.loads(line) for line in out.splitlines() if line]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--count", type=int, default=25000, help="devices per date")
    parser.add_argument("--cxx", default="clang++", help="C++23 compiler")
    args = parser.parse_args()

    devices = [d for d in sample(args.count, args.cxx) if d["user_agent"]]
    checks = collections.Counter()
    misses = collections.Counter()
    for d in devices:
        r = parse(d["user_agent"])
        ua, os_, dev = r.user_agent, r.os, r.device
        family_ok = ua is not None and ua.family in BROWSER_FAMILIES[d["browser"]]
        major_ok = family_ok and ua.major == d["browser_version"].split(".")[0]
        os_ok = os_ is not None and os_.family in OS_FAMILIES[d["os"]]
        checks["browser family"] += family_ok
        checks["browser major"] += major_ok
        checks["os family"] += os_ok
        if d["maker"] and d["os"] in ("Android", "iOS"):
            checks["brand checked"] += 1
            brand_ok = dev is not None and (dev.brand or "").lower() == d["maker"].lower()
            reduced = "; K)" in d["user_agent"]
            checks["brand (model token sent)"] += brand_ok and not reduced
            checks["model token sent"] += not reduced
        if not (family_ok and major_ok and os_ok):
            what = (d["browser"], d["os"], ua.family if ua else None, os_.family if os_ else None)
            misses[what] += 1

    n = len(devices)
    print(f"{n} user agents at {', '.join(DATES)}")
    for key in ("browser family", "browser major", "os family"):
        print(f"  {key}: {checks[key] / n:.2%}")
    if checks["model token sent"]:
        print(f"  device brand, where a model token is sent: "
              f"{checks['brand (model token sent)'] / checks['model token sent']:.2%} "
              f"of {checks['model token sent']}")
    if misses:
        print("disagreements (generator browser, generator OS, parser browser, parser OS): count")
        for what, count in misses.most_common(12):
            print(f"  {what}: {count}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
