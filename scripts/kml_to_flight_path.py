"""Converts a KML flight into the C table the fake telemetry plays back.

PlatformIO runs this before every build (extra_scripts in platformio.ini) for the KML set by
the custom_flight_path option, and puts the generated flight_path_data.h on the include path.
It can also be run by hand:

    python scripts/kml_to_flight_path.py data/madrid-demo-flight.kml flight_path_data.h

The KML needs one gx:Track with, for every point:
  <when>                  timestamp, strictly increasing
  <gx:coord>              longitude latitude [altitude] (m, above mean sea level, 0 when left out)
and optionally, also for every point:
  <gx:angles>             heading = yaw (deg), tilt = pitch (deg), roll (deg)
  <gx:SimpleArrayData>    one array per field in FIELDS below
Anything left out is derived from the track (see derive_missing), so a plain GPS track works.
Placemarks with a <Point> and a <TimeStamp> are waypoints: each track point gets the next
waypoint (by time) as its target, shown on the telemetry page.
"""

import math
import os
import re
import sys
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta, timezone

KML_NS = "{http://www.opengis.net/kml/2.2}"
GX_NS = "{http://www.google.com/kml/ext/2.2}"

DEFAULT_FLIGHT_PATH = "data/madrid-demo-flight.kml"
HEADER_NAME = "flight_path_data.h"

# name -> kind, in the order of flight_path_point_t (src/flight_path.cpp)
FIELDS = [
    ("ground_speed", "float"),       # m/s
    ("course", "float"),             # deg, course over ground
    ("vertical_speed", "float"),     # m/s
    ("baro_altitude", "float"),      # m above takeoff
    ("battery_voltage", "float"),    # V
    ("battery_current", "float"),    # A
    ("battery_used", "float"),       # mAh
    ("battery_remaining", "uint8"),  # %
    ("satellites", "uint8"),
    ("flight_mode", "string"),       # sent in the CRSF flight mode frame
    ("phase", "string"),             # telemetry page only
]
FLIGHT_MODE_MAX_LEN = 15  # CRSF_TX_FLIGHT_MODE_MAX_LEN in src/crsf_tx.h

# Used for fields a track doesn't have
EARTH_RADIUS_M = 6371000.0
GRAVITY = 9.81
DIFFERENCE_WINDOW_S = 2.0   # speeds and accelerations span at least this, damps GPS noise
MOVING_MPS = 1.0            # slower holds the course (GPS noise would spin it) and doesn't count as flying
AIRBORNE_ALTITUDE_M = 2.0   # higher above takeoff counts as flying
MAX_TILT_DEG = 45.0
DEFAULT_SATELLITES = 12
DEFAULT_FLIGHT_MODE = "ANGL"
# Same 4S 3000 mAh pack as the Madrid demo
BATTERY_CELLS = 4
BATTERY_CAPACITY_MAH = 3000.0
CELL_SOC = [0.0, 0.10, 0.30, 0.60, 0.90, 1.0]
CELL_VOLTS = [3.30, 3.55, 3.72, 3.80, 4.00, 4.20]
GROUND_CURRENT_A = 0.4

TIME_RE = re.compile(
    r"^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(\.\d+)?(Z|[+-]\d{2}:\d{2})?$")


class FlightPathError(Exception):
    pass


def parse_time(text):
    m = TIME_RE.match(text.strip())
    if not m:
        raise FlightPathError("unsupported <when> timestamp '%s', expected YYYY-MM-DDThh:mm:ss[.fff][Z|+hh:mm]" % text)
    year, month, day, hour, minute, second = (int(g) for g in m.groups()[:6])
    tz = timezone.utc
    if m.group(8) and m.group(8) != "Z":
        sign = 1 if m.group(8)[0] == "+" else -1
        tz = timezone(sign * timedelta(hours=int(m.group(8)[1:3]), minutes=int(m.group(8)[4:6])))
    t = datetime(year, month, day, hour, minute, second, tzinfo=tz).timestamp()
    return t + (float(m.group(7)) if m.group(7) else 0.0)


def parse_numbers(text, counts, what):
    try:
        values = [float(v) for v in text.split()]
    except ValueError:
        values = []
    if len(values) not in counts or not all(math.isfinite(v) for v in values):
        raise FlightPathError("%s '%s' needs %s numbers" % (what, text.strip(), " or ".join(str(c) for c in counts)))
    return values


def read_kml(path):
    try:
        root = ET.parse(path).getroot()
    except (OSError, ET.ParseError) as e:
        raise FlightPathError("cannot read %s: %s" % (path, e))
    tracks = list(root.iter(GX_NS + "Track"))
    if len(tracks) != 1:
        raise FlightPathError("expected exactly one gx:Track, found %d" % len(tracks))
    track = tracks[0]

    times = [parse_time(e.text or "") for e in track.findall(KML_NS + "when")]
    coords = [(parse_numbers(e.text or "", (2, 3), "gx:coord") + [0.0])[:3] for e in track.findall(GX_NS + "coord")]
    angles = [parse_numbers(e.text or "", (3,), "gx:angles") for e in track.findall(GX_NS + "angles")]
    if len(times) < 2:
        raise FlightPathError("the track needs at least 2 points")
    if len(coords) != len(times):
        raise FlightPathError("%d <when> but %d gx:coord" % (len(times), len(coords)))
    if angles and len(angles) != len(times):
        raise FlightPathError("%d <when> but %d gx:angles" % (len(times), len(angles)))
    for a, b in zip(times, times[1:]):
        if b <= a:
            raise FlightPathError("<when> timestamps must be strictly increasing")

    arrays = {}
    for e in track.iter(GX_NS + "SimpleArrayData"):
        arrays[e.get("name")] = [(v.text or "").strip() for v in e.findall(GX_NS + "value")]
    fields = {}
    for name, kind in FIELDS:
        if name not in arrays:
            continue
        values = arrays[name]
        if len(values) != len(times):
            raise FlightPathError("%d <when> but %d values in %s" % (len(times), len(values), name))
        try:
            if kind == "float":
                values = [float(v) for v in values]
                if not all(math.isfinite(v) for v in values):
                    raise ValueError
            elif kind == "uint8":
                values = [int(v) for v in values]
                if not all(0 <= v <= 255 for v in values):
                    raise ValueError
        except ValueError:
            raise FlightPathError("%s needs %s values" % (name, "0-255 integer" if kind == "uint8" else kind))
        fields[name] = values

    waypoints = []
    for placemark in root.iter(KML_NS + "Placemark"):
        when = placemark.find(KML_NS + "TimeStamp/" + KML_NS + "when")
        if placemark.find(KML_NS + "Point") is not None and when is not None:
            waypoints.append((parse_time(when.text or ""), (placemark.findtext(KML_NS + "name") or "").strip()))
    waypoints.sort()

    return times, coords, angles, fields, waypoints


def distance_bearing(a, b):
    """Great circle distance (m) and initial bearing (deg, 0..360) from a to b, both (lon, lat, ...)."""
    lat1, lat2 = math.radians(a[1]), math.radians(b[1])
    dlat, dlon = lat2 - lat1, math.radians(b[0] - a[0])
    h = math.sin(dlat / 2) ** 2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2) ** 2
    distance = 2 * EARTH_RADIUS_M * math.asin(math.sqrt(min(1.0, h)))
    bearing = math.atan2(math.sin(dlon) * math.cos(lat2),
                         math.cos(lat1) * math.sin(lat2) - math.sin(lat1) * math.cos(lat2) * math.cos(dlon))
    return distance, math.degrees(bearing) % 360.0


def neighbours(times, i):
    """Points around i spanning about DIFFERENCE_WINDOW_S for central differences, one-sided at the ends."""
    j, k = i, i
    while j > 0 and times[i] - times[j] < DIFFERENCE_WINDOW_S / 2:
        j -= 1
    while k < len(times) - 1 and times[k] - times[i] < DIFFERENCE_WINDOW_S / 2:
        k += 1
    return j, k


def cell_voltage(remaining):
    for i in range(1, len(CELL_SOC)):
        if remaining <= CELL_SOC[i]:
            u = (remaining - CELL_SOC[i - 1]) / (CELL_SOC[i] - CELL_SOC[i - 1])
            return CELL_VOLTS[i - 1] + u * (CELL_VOLTS[i] - CELL_VOLTS[i - 1])
    return CELL_VOLTS[-1]


def derive_missing(times, coords, angles, fields):
    """Fills in fields and angles the KML doesn't have, returns the names of the derived ones."""
    n = len(times)
    derived = []

    def need(name, values):
        if name not in fields:
            fields[name] = values()
            derived.append(name)

    def ground_speeds():
        speeds = []
        for i in range(n):
            j, k = neighbours(times, i)
            speeds.append(distance_bearing(coords[j], coords[k])[0] / (times[k] - times[j]))
        return speeds

    def courses():
        result = []
        for i in range(n):
            j, k = neighbours(times, i)
            result.append(distance_bearing(coords[j], coords[k])[1] if fields["ground_speed"][i] >= MOVING_MPS else None)
        known = [c for c in result if c is not None]
        held = known[0] if known else 0.0  # before the first move, use its course
        for i in range(n):
            held = result[i] if result[i] is not None else held
            result[i] = held
        return result

    def vertical_speeds():
        result = []
        for i in range(n):
            j, k = neighbours(times, i)
            result.append((coords[k][2] - coords[j][2]) / (times[k] - times[j]))
        return result

    need("ground_speed", ground_speeds)
    need("course", courses)
    need("vertical_speed", vertical_speeds)
    need("baro_altitude", lambda: [c[2] - coords[0][2] for c in coords])
    airborne = [fields["baro_altitude"][i] > AIRBORNE_ALTITUDE_M or fields["ground_speed"][i] > MOVING_MPS
                for i in range(n)]

    if not angles:
        # Yaw follows the course. Pitch and roll tilt the thrust to make the horizontal
        # acceleration: forward accelerating pitches nose down, turning right rolls right.
        velocity = [(s * math.sin(math.radians(c)), s * math.cos(math.radians(c)))
                    for s, c in zip(fields["ground_speed"], fields["course"])]
        for i in range(n):
            j, k = neighbours(times, i)
            dt = times[k] - times[j]
            east = (velocity[k][0] - velocity[j][0]) / dt
            north = (velocity[k][1] - velocity[j][1]) / dt
            course = math.radians(fields["course"][i])
            forward = east * math.sin(course) + north * math.cos(course)
            right = east * math.cos(course) - north * math.sin(course)
            pitch = max(-MAX_TILT_DEG, min(MAX_TILT_DEG, -math.degrees(math.atan2(forward, GRAVITY))))
            roll = max(-MAX_TILT_DEG, min(MAX_TILT_DEG, math.degrees(math.atan2(right, GRAVITY))))
            angles.append([fields["course"][i], pitch, roll])
        derived.append("gx:angles")

    # More current when flying faster or climbing
    need("battery_current", lambda: [
        12.0 + 0.3 * fields["ground_speed"][i] + 2.0 * max(0.0, fields["vertical_speed"][i])
        if airborne[i] else GROUND_CURRENT_A for i in range(n)])

    def used():
        result = [0.0]
        current = fields["battery_current"]
        for i in range(1, n):
            mah = (current[i - 1] + current[i]) / 2 * (times[i] - times[i - 1]) / 3.6
            result.append(min(BATTERY_CAPACITY_MAH, result[-1] + mah))
        return result

    need("battery_used", used)
    need("battery_remaining", lambda: [
        int(round(100 * max(0.0, 1 - mah / BATTERY_CAPACITY_MAH))) for mah in fields["battery_used"]])

    def voltages():
        result = []
        for i in range(n):
            remaining = fields["battery_remaining"][i] / 100.0
            resistance = 0.045 + 0.025 * (1 - remaining)  # ohm, rises as the pack empties
            result.append(BATTERY_CELLS * cell_voltage(remaining) - fields["battery_current"][i] * resistance)
        return result

    need("battery_voltage", voltages)
    need("satellites", lambda: [DEFAULT_SATELLITES] * n)
    need("flight_mode", lambda: [DEFAULT_FLIGHT_MODE] * n)
    need("phase", lambda: ["FLYING" if a else "LANDED" for a in airborne])
    return derived


def c_string(text):
    if not all(32 <= ord(c) < 127 for c in text):
        raise FlightPathError("only printable ASCII is supported in strings: '%s'" % text)
    return '"%s"' % text.replace("\\", "\\\\").replace('"', '\\"')


def c_float(value):
    text = repr(round(float(value), 6))
    return text + ("f" if ("." in text or "e" in text) else ".0f")


def generate_header(kml_path, source_name):
    """Returns the header text and the names of the derived fields."""
    times, coords, angles, fields, waypoints = read_kml(kml_path)
    derived = derive_missing(times, coords, angles, fields)
    for mode in fields["flight_mode"]:
        if len(mode) > FLIGHT_MODE_MAX_LEN:
            raise FlightPathError("flight_mode '%s' is longer than %d characters" % (mode, FLIGHT_MODE_MAX_LEN))
    duration_ms = round((times[-1] - times[0]) * 1000)
    if duration_ms > 0xFFFFFFFF:
        raise FlightPathError("the flight is too long")

    strings = [""]
    def string_index(text):
        if text not in strings:
            strings.append(text)
        if len(strings) > 256:
            raise FlightPathError("too many different strings (flight modes, phases, waypoint names)")
        return strings.index(text)

    rows = []
    for i, t in enumerate(times):
        target = next((name for wt, name in waypoints if wt > t), waypoints[-1][1] if waypoints else "")
        lon, lat, alt = coords[i]
        heading, tilt, roll = angles[i]
        row = [
            str(round((t - times[0]) * 1000)),
            str(round(lat * 1e7)),
            str(round(lon * 1e7)),
            c_float(alt),
            c_float(heading),
            c_float(tilt),
            c_float(roll),
        ]
        for name, kind in FIELDS:
            value = fields[name][i]
            row.append(c_float(value) if kind == "float" else str(string_index(value) if kind == "string" else value))
        row.append(str(string_index(target)))
        rows.append("\t{" + ", ".join(row) + "},")

    text = "\n".join([
        "// Generated by scripts/kml_to_flight_path.py from %s, do not edit." % source_name,
        "// %d points, %.1f s. Derived: %s. Included by src/flight_path.cpp." % (
            len(rows), duration_ms / 1000.0, ", ".join(derived) if derived else "nothing"),
        "#pragma once",
        "",
        "static const char * const FLIGHT_PATH_STRINGS [] = {",
        "\t" + ", ".join(c_string(s) for s in strings),
        "};",
        "",
        "static const flight_path_point_t FLIGHT_PATH_POINTS [] = {",
        *rows,
        "};",
        "",
    ])
    return text, derived


def write_if_changed(path, text):
    try:
        with open(path, "r", encoding="ascii") as f:
            if f.read() == text:
                return
    except OSError:
        pass
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write(text)


def describe(flight_path, derived):
    return "Flight path: %s%s" % (flight_path, " (derived: %s)" % ", ".join(derived) if derived else "")


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: python %s flight.kml %s" % (sys.argv[0], HEADER_NAME))
    try:
        text, derived = generate_header(sys.argv[1], sys.argv[1])
    except FlightPathError as e:
        sys.exit("%s: %s" % (sys.argv[1], e))
    write_if_changed(sys.argv[2], text)
    print(describe(sys.argv[1], derived))


try:
    Import("env")  # noqa: F821 - only defined when PlatformIO runs this as an extra script
except NameError:
    env = None

if env is not None:
    flight_path = env.GetProjectOption("custom_flight_path", DEFAULT_FLIGHT_PATH)
    kml_path = os.path.join(env.subst("$PROJECT_DIR"), flight_path)
    out_dir = os.path.join(env.subst("$BUILD_DIR"), "flight_path")
    try:
        text, derived = generate_header(kml_path, flight_path)
    except FlightPathError as e:
        sys.stderr.write("Error: flight path %s: %s\n" % (flight_path, e))
        env.Exit(1)
    write_if_changed(os.path.join(out_dir, HEADER_NAME), text)
    print(describe(flight_path, derived))
    env.Append(CPPPATH=[out_dir])
elif __name__ == "__main__":
    main()
