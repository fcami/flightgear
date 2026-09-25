#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 James Turner
# SPDX-License-Identifier: GPL-2.0-or-later

"""Generate synthetic Level-D procedures XML for the FlightGear test suite.

Reads specs/<ICAO>.json and writes Airports/<X>/<Y>/<Z>/<ICAO>.procedures.xml.
Real fixes, navaids and runway thresholds are resolved from FGData;
everything else is invented. See README.md for the spec format.

Usage:
    generate.py --fg-root /path/to/fgdata [ICAO ...]
    generate.py --fg-root /path/to/fgdata --check   # verify outputs are current
"""

import argparse
import gzip
import json
import math
import os
import re
import sys
from pathlib import Path
from xml.sax.saxutils import escape, quoteattr

HERE = Path(__file__).resolve().parent
SPECS_DIR = HERE / "specs"
EARTH_RADIUS_NM = 3440.065

# written into every generated file; fixed rather than the current year so
# --check doesn't report every file as stale once the year changes
COPYRIGHT = "2026 Curtis L Olson"
LICENSE = "GPL-2.0-or-later"

# invented fixes must not collide with a real fix / navaid this close to the airport
CLASH_RADIUS_NM = 300.0
# real fixes / navaids are only accepted if this close to the airport
LOOKUP_RADIUS_NM = 400.0


class SpecError(Exception):
    pass


def distance_nm(a, b):
    lat1, lon1 = map(math.radians, a)
    lat2, lon2 = map(math.radians, b)
    h = math.sin((lat2 - lat1) / 2) ** 2 + \
        math.cos(lat1) * math.cos(lat2) * math.sin((lon2 - lon1) / 2) ** 2
    return 2 * EARTH_RADIUS_NM * math.asin(math.sqrt(h))


def bearing(a, b):
    lat1, lon1 = map(math.radians, a)
    lat2, lon2 = map(math.radians, b)
    y = math.sin(lon2 - lon1) * math.cos(lat2)
    x = math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(lon2 - lon1)
    return math.degrees(math.atan2(y, x)) % 360


def offset(pos, bearing_deg, dist_nm):
    """Spherical direct problem: accuracy is irrelevant for invented fixes."""
    lat1, lon1 = map(math.radians, pos)
    brg = math.radians(bearing_deg)
    d = dist_nm / EARTH_RADIUS_NM
    lat2 = math.asin(math.sin(lat1) * math.cos(d) +
                     math.cos(lat1) * math.sin(d) * math.cos(brg))
    lon2 = lon1 + math.atan2(math.sin(brg) * math.sin(d) * math.cos(lat1),
                             math.cos(d) - math.sin(lat1) * math.sin(lat2))
    return (math.degrees(lat2), (math.degrees(lon2) + 540) % 360 - 180)


class FGData:
    """The subset of FGData navdata needed to resolve the specs."""

    def __init__(self, fg_root, icaos):
        self.root = Path(fg_root)
        self.points = {}  # ident -> [(lat, lon)]
        self.runways = {}  # icao -> {ident: (lat, lon)}
        self.headings = {}  # icao -> {ident: true heading}
        self._load_points(self.root / "Navaids" / "fix.dat.gz", ident_col=2, lat_col=0, lon_col=1)
        # nav.dat: NDB (2), VOR (3) and DME (12, 13) rows only
        self._load_points(self.root / "Navaids" / "nav.dat.gz", ident_col=7, lat_col=1, lon_col=2,
                          row_codes={"2", "3", "12", "13"})
        self._load_runways(self.root / "Airports" / "apt.dat.gz", set(icaos))

    def _load_points(self, path, ident_col, lat_col, lon_col, row_codes=None):
        with gzip.open(path, "rt", encoding="latin-1") as f:
            for line in f:
                p = line.split()
                if len(p) <= ident_col or (row_codes and p[0] not in row_codes):
                    continue
                try:
                    pos = (float(p[lat_col]), float(p[lon_col]))
                except ValueError:
                    continue
                self.points.setdefault(p[ident_col], []).append(pos)

    def _load_runways(self, path, icaos):
        current = None
        with gzip.open(path, "rt", encoding="latin-1") as f:
            for line in f:
                p = line.split()
                if not p:
                    continue
                if p[0] in ("1", "16", "17"):
                    current = p[4] if p[4] in icaos else None
                    if current:
                        self.runways[current] = {}
                        self.headings[current] = {}
                    elif len(self.runways) == len(icaos):
                        break
                elif p[0] == "100" and current:
                    ends = [(p[b], (float(p[b + 1]), float(p[b + 2]))) for b in (8, 17)]
                    for (ident, pos), (_, other) in (ends, ends[::-1]):
                        self.runways[current][ident] = pos
                        self.headings[current][ident] = bearing(pos, other)

    def nearest(self, ident, near):
        cands = self.points.get(ident, [])
        if not cands:
            return None
        best = min(cands, key=lambda c: distance_nm(c, near))
        return best if distance_nm(best, near) <= LOOKUP_RADIUS_NM else None


class AirportGen:
    def __init__(self, spec, fgdata):
        self.icao = spec["airport"]
        self.spec = spec
        self.fgdata = fgdata
        if self.icao not in fgdata.runways:
            raise SpecError(f"{self.icao}: airport not found in apt.dat")
        self.runways = fgdata.runways[self.icao]
        self.headings = fgdata.headings[self.icao]
        lats, lons = zip(*self.runways.values())
        self.ref = (sum(lats) / len(lats), sum(lons) / len(lons))
        self.invented = {}  # name -> (lat, lon)

    # ---- position resolution ----------------------------------------------

    def position_of(self, name):
        if name == self.icao:
            return self.ref
        if name.startswith("RW") and name[2:] in self.runways:
            return self.runways[name[2:]]
        if name in self.invented:
            return self.invented[name]
        pos = self.fgdata.nearest(name, self.ref)
        if pos is None:
            raise SpecError(f"{self.icao}: unknown fix / navaid / runway '{name}'")
        return pos

    def define(self, name, definition):
        m = re.fullmatch(r"(-?[\d.]+),(-?[\d.]+)", definition)
        if m:
            pos = (float(m.group(1)), float(m.group(2)))
        else:
            m = re.fullmatch(r"([A-Z0-9]+)/([\d.]+|CL)/([\d.]+)", definition)
            if not m:
                raise SpecError(f"{self.icao}: bad definition '{name}={definition}'")
            ref, brg = m.group(1), m.group(2)
            if brg == "CL":
                # along the extended centreline, in the runway's direction
                if not (ref.startswith("RW") and ref[2:] in self.headings):
                    raise SpecError(f"{self.icao}: 'CL' bearing needs a runway reference in '{name}'")
                brg = self.headings[ref[2:]]
            pos = offset(self.position_of(ref), float(brg), float(m.group(3)))

        if name in self.invented and distance_nm(self.invented[name], pos) > 0.01:
            raise SpecError(f"{self.icao}: '{name}' defined twice with different positions")
        if self.fgdata.nearest(name, self.ref) is not None and \
                distance_nm(self.fgdata.nearest(name, self.ref), self.ref) <= CLASH_RADIUS_NM:
            raise SpecError(f"{self.icao}: invented fix '{name}' clashes with a real FGData ident")
        self.invented[name] = pos
        return pos

    # ---- waypoint building --------------------------------------------------

    def waypoint(self, leg, prev_pos):
        """Return (list of (element, value), position) for one leg spec."""
        if isinstance(leg, str):
            leg = {"wp": leg}

        kind = leg.get("type", "fix")
        minimal = leg.get("minimal", False)
        name, wtype, pos = None, "Normal", prev_pos
        extra = []  # type-specific elements, in Level-D order

        altitude, restriction = 0, "at"
        if "alt" in leg:
            alt = str(leg["alt"])
            restriction = {"+": "above", "-": "below"}.get(alt[0], "at")
            altitude = int(alt.lstrip("+-"))

        if kind in ("fix", "hold"):
            spec = leg["wp"]
            name, _, definition = spec.partition("=")
            if definition:
                pos = self.define(name, definition)
            else:
                pos = self.position_of(name)
            if kind == "fix" and name.startswith("RW") and name[2:] in self.runways and not definition:
                wtype = "Runway"
            if kind == "hold":
                # hold elements are only written when given, so that specs can
                # exercise the parser defaults
                wtype = "Hold"
                if "inbound" in leg:
                    extra.append(("Hld_Rad_value", leg["inbound"]))
                if "turn" in leg:
                    extra.append(("Hld_Turn", {"L": "Left", "R": "Right"}[leg["turn"]]))
                if "distance" in leg:
                    extra += [("Hld_Time_or_Dist", "Dist"), ("Hld_td_value", leg["distance"])]
                elif "time" in leg:
                    extra += [("Hld_Time_or_Dist", "Time"), ("Hld_td_value", leg["time"])]
        elif kind == "hdg_alt":
            if "alt" not in leg:
                raise SpecError(f"{self.icao}: hdg_alt leg needs an 'alt'")
            wtype = "ConstHdgtoAlt"
            name = leg.get("name", f"({altitude})")
            extra += [("Hdg_Crs", 0), ("Hdg_Crs_value", leg["heading"])]
        elif kind == "vectors":
            wtype = "Vectors"
            name = leg.get("name", "(VECTORS)")
        elif kind == "pbd":
            # Level-D PBD: position of the base fix, bearing and distance (nm) from it
            wtype = "PBD"
            name = leg["name"]
            pos = self.position_of(leg["base"])
            extra += [("Hdg_Crs", 1), ("Hdg_Crs_value", leg["bearing"]),
                      ("DMEtoIntercept", leg["distance"])]
        elif kind == "dme_intc":
            wtype = "DmeIntc"
            name = leg["name"]
            pos = self.position_of(leg["navaid"])
            extra += [("Hdg_Crs", 1), ("Hdg_Crs_value", leg["course"]),
                      ("DMEtoIntercept", leg["distance"])]
        elif kind == "radial_intc":
            wtype = "Intc"
            name = leg["name"]
            pos = self.position_of(leg["navaid"])
            extra += [("Hdg_Crs", 1), ("Hdg_Crs_value", leg["course"]),
                      ("RadialtoIntercept", leg["radial"])]
        else:
            raise SpecError(f"{self.icao}: unsupported leg type '{kind}'")

        fields = [("Name", escape(name)), ("Type", wtype),
                  ("Latitude", f"{pos[0]:.6f}"), ("Longitude", f"{pos[1]:.6f}")]
        if minimal:
            # only what the spec states explicitly
            if "speed" in leg:
                fields.append(("Speed", leg["speed"]))
            if "alt" in leg:
                fields += [("Altitude", altitude), ("AltitudeRestriction", restriction)]
            fields += extra
            if "flyover" in leg:
                fields.append(("Flytype", "Fly-over" if leg["flyover"] else "Fly-by"))
        else:
            fields += [("Speed", leg.get("speed", 0)), ("Altitude", altitude),
                       ("AltitudeCons", 0), ("AltitudeRestriction", restriction)]
            fields += extra
            fields += [("Flytype", "Fly-over" if leg.get("flyover") else "Fly-by"),
                       ("BankLimit", 25), ("Sp_Turn", "Auto")]
        return fields, pos

    def waypoints_xml(self, tag, legs, indent, start_pos=None):
        out = []
        pos = start_pos or self.ref
        pad = " " * indent
        for i, leg in enumerate(legs, start=1):
            fields, pos = self.waypoint(leg, pos)
            out.append(f'{pad}<{tag} ID="{i}">')
            out += [f"{pad}  <{k}>{v}</{k}>" for k, v in fields]
            out.append(f"{pad}</{tag}>")
        return out

    # ---- procedures ---------------------------------------------------------

    def check_runways(self, proc):
        for r in proc.get("runways", []):
            if r not in self.runways:
                raise SpecError(f"{self.icao}: {proc['name']} uses unknown runway {r}")

    def arr_dep_xml(self, proc, kind):
        tag, wp_tag, tr_tag, trwp_tag = {
            "sid": ("Sid", "Sid_Waypoint", "Sid_Transition", "SidTr_Waypoint"),
            "star": ("Star", "Star_Waypoint", "Star_Transition", "StarTr_Waypoint"),
        }[kind]
        self.check_runways(proc)
        attrs = f"Name={quoteattr(proc['name'])}"
        if proc.get("runways"):
            attrs += f" Runways={quoteattr(','.join(proc['runways']))}"

        out = [f"    <{tag} {attrs}>"]
        out += self.waypoints_xml(wp_tag, proc["legs"], 6)
        for t in proc.get("transitions", []):
            out.append(f"      <{tr_tag} Name={quoteattr(t['name'])}>")
            out += self.waypoints_xml(trwp_tag, t["legs"], 8)
            out.append(f"      </{tr_tag}>")
        for t in proc.get("runway_transitions", []):
            if t["runway"] not in self.runways:
                raise SpecError(f"{self.icao}: runway transition for unknown runway {t['runway']}")
            out.append(f"      <RunwayTransition Runway={quoteattr(t['runway'])}>")
            out += self.waypoints_xml("RwyTr_Waypoint", t["legs"], 8)
            out.append("      </RunwayTransition>")
        out.append(f"    </{tag}>")
        return out

    def approach_xml(self, proc):
        out = [f"    <Approach Name={quoteattr(proc['name'])}>"]
        out += self.waypoints_xml("App_Waypoint", proc["legs"], 6)
        for t in proc.get("transitions", []):
            out.append(f"      <App_Transition Name={quoteattr(t['name'])}>")
            out += self.waypoints_xml("AppTr_Waypoint", t["legs"], 8)
            out.append("      </App_Transition>")
        out.append("    </Approach>")
        return out

    def generate(self):
        out = ['<?xml version="1.0" encoding="UTF-8"?>',
               "<!--",
               f"  SPDX-FileCopyrightText: {COPYRIGHT}",
               f"  SPDX-License-Identifier: {LICENSE}",
               "",
               f"  SYNTHETIC procedures for the FlightGear test suite: NOT FOR NAVIGATION.",
               f"  Generated by generate.py from specs/{self.icao}.json - do not edit by hand.",
               "  Only fix, navaid and runway positions come from FGData; all procedure",
               "  names, routings and restrictions are invented for testing.",
               "-->",
               '<ProceduresDB CycleDate="SYNTHETIC">',
               f"  <Airport ICAOcode={quoteattr(self.icao)}>"]
        # define invented fixes up-front so they may be referenced in any order
        for name, definition in self.spec.get("fixes", {}).items():
            self.define(name, definition)
        for p in self.spec.get("sids", []):
            out += self.arr_dep_xml(p, "sid")
        for p in self.spec.get("stars", []):
            out += self.arr_dep_xml(p, "star")
        for p in self.spec.get("approaches", []):
            out += self.approach_xml(p)
        out += ["  </Airport>", "</ProceduresDB>", ""]
        return "\n".join(out)


def output_path(icao):
    return HERE / "Airports" / icao[0] / icao[1] / icao[2] / f"{icao}.procedures.xml"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--fg-root", default=os.environ.get("FG_ROOT"), help="FGData directory (default: $FG_ROOT)")
    ap.add_argument("--check", action="store_true", help="fail if any generated file is out of date")
    ap.add_argument("airports", nargs="*", help="airports to generate (default: all specs)")
    args = ap.parse_args()

    if not args.fg_root:
        ap.error("--fg-root or $FG_ROOT is required")

    icaos = args.airports or sorted(p.stem for p in SPECS_DIR.glob("*.json"))
    specs = {i: json.loads((SPECS_DIR / f"{i}.json").read_text()) for i in icaos}
    fgdata = FGData(args.fg_root, icaos)

    stale = []
    for icao, spec in specs.items():
        try:
            xml = AirportGen(spec, fgdata).generate()
        except SpecError as e:
            sys.exit(f"error: {e}")
        path = output_path(icao)
        if args.check:
            if not path.exists() or path.read_text() != xml:
                stale.append(icao)
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(xml)
        print(f"wrote {path.relative_to(HERE)}")

    if stale:
        sys.exit("out of date: " + ", ".join(stale) + " (re-run generate.py)")


if __name__ == "__main__":
    main()
