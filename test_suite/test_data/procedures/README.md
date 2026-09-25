# Synthetic procedures for the test suite

**Not for navigation.** These SIDs, STARs and approaches are invented. They are
plausible enough to exercise FlightGear's procedure code, but they are not
real procedures and are not aeronautically valid.

## Provenance and licence

Everything here is GPL-2.0-or-later.

- Fix, navaid and runway-threshold positions come from FGData
  (`Navaids/fix.dat.gz`, `Navaids/nav.dat.gz`, `Airports/apt.dat.gz`).
- Procedure names, routings, invented fixes and restrictions are written by
  hand in `specs/`, working from what the tests need.

`FGTestApi::setUp::initNavDataCache()` adds this directory as a scenery path, so
every test that uses the nav cache sees these procedures.

## Regenerating

```sh
python3 generate.py --fg-root /path/to/fgdata          # all airports
python3 generate.py --fg-root /path/to/fgdata EHAM     # one airport
python3 generate.py --fg-root /path/to/fgdata --check  # fail if stale
```

Commit the spec and the generated XML together.

## Spec format

```json
{
  "airport": "KJFK",
  "comment": ["free text: which tests depend on what"],
  "fixes": { "JFS01": "KJFK/150/6" },
  "sids": [ {
    "name": "...", "runways": ["13L"], "legs": [...],
    "transitions": [ { "name": "...", "legs": [...] } ],
    "runway_transitions": [ { "runway": "13L", "legs": [...] } ]
  } ],
  "stars": [ same as sids ],
  "approaches": [ {
    "name": "ILS06", "legs": [...],
    "transitions": [ { "name": "...", "legs": [...] } ]
  } ]
}
```

Omitting `runways` means the procedure applies to all runways. Legs are listed
in flight order for every procedure type. The parser reverses SIDs internally.

An approach's legs must include its runway (`"RW06"`). Legs after the runway
are the missed approach. The approach type comes from the name prefix: `ILS`,
`VOR`/`VDM`, `NDB`/`NDM`, otherwise RNAV.

### Fixes

The `fixes` table defines invented fixes, as either:

- `"REF/bearing/distance"`: true bearing in degrees and distance in nm from
  `REF`. `REF` is the airport ident (its runway centroid), a runway (`RW13L`,
  its threshold), a real FGData fix or navaid, or another invented fix. With a
  runway `REF`, the bearing can be `CL`, the runway heading from apt.dat:
  `"RW36/CL/4"` is 4 nm along the extended centreline beyond the runway 36
  threshold.
- `"lat,lon"`: decimal degrees.

Some tests fly procedures with the test pilot and GPS, or assert distances along
them (EGPH, LFKC, LEBL, VQPR), so their geometry matters: read the spec's
`comment` before moving fixes.

Invented names must not match a real FGData ident within 300 nm. Otherwise
`waypointFromString()` and matching by ident would be ambiguous.

### Legs

A leg is either a string or an object:

- `"CANDR"`: `Normal`. An invented fix, or the nearest real FGData fix or
  navaid within 400 nm.
- `"NAME=REF/brg/dist"`: `Normal`. Defines an invented fix inline.
- `"RW13L"`: `Runway`. Must exist in apt.dat.
- `{"wp": "...", "alt": "+5000", "speed": 250, "flyover": true}`: as above,
  with restrictions. `alt` is `"5000"` (at), `"+5000"` (at or above) or
  `"-5000"` (at or below).
- `{"type": "hdg_alt", "heading": 121, "alt": "+600", "name": "(600)"}`:
  `ConstHdgtoAlt`. The default name is `(<alt>)`.
- `{"type": "vectors"}`: `Vectors`.
- `{"type": "hold", "wp": "FIX", "inbound": 130, "turn": "L", "distance": 5}`:
  `Hold`. Use `"time": 1.5` (minutes) instead of `distance` for a timed hold.
  Hold elements are only written when given.
- `{"type": "pbd", "name": "SAM05", "base": "SAM", "bearing": 90,
  "distance": 5}`: `PBD`, a place / bearing / distance (nm) from a fix.
- `{"type": "dme_intc", "name": "...", "navaid": "SAM", "course": 200,
  "distance": 12}`: `DmeIntc`.
- `{"type": "radial_intc", "name": "...", "navaid": "SAM", "course": 270,
  "radial": 180}`: `Intc`.

Add `"minimal": true` to any leg to write only the required elements (name,
type, position) plus whatever the leg states explicitly. Use it to check that
the parser doesn't carry values over from the previous waypoint.

Real fixes the tests look up by identity (for example SAM in
`RNAVProcedureTests::testIndexOf`) must be referenced by name, so they get the
exact FGData position. Waypoints without a source only match within 100 m.
