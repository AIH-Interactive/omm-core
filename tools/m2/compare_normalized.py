"""Compares NormalizedDump (OMP Java) and omm_ysm_normalized_dump (native) outputs.

Rules:
  * every line must match exactly, except
  * QUAD and BONE float fields (computed geometry) may differ by evaluation-order rounding:
    |a - b| <= TOLERANCE * max(1, |a|, |b|). Coordinates are model units (1/16 block) and
    intermediates are pivot-relative, so an error bounded by the larger magnitude, or by
    TOLERANCE absolutely near zero, is the correct metric; 1e-6 block is far below render precision.
  * one documented transformation on TEXTURE lines: OMP transcodes v3 textures to
    "<stem>.png", the native mapper keeps "<stem>.encoded" (the host decodes by media type).

Usage: python compare_normalized.py <java-dump> <native-dump>
Exit code 0 when every sample matches under these rules.
"""

import re
import struct
import sys
from itertools import zip_longest

TOLERANCE = 1e-6
TEXTURE_REWRITE = re.compile(r"(textures/[^\t|]*?)\.png")


def read_samples(path):
    current = None
    lines = []
    seen = set()
    with open(path, encoding="utf-8", newline="") as handle:
        for raw in handle:
            line = raw.rstrip("\r\n")
            if line.startswith("SAMPLE\t"):
                if current is not None:
                    yield current, lines
                current = line.split("\t", 1)[1]
                if current in seen:
                    raise ValueError(f"duplicate sample: {current}")
                seen.add(current)
                lines = []
            elif current is not None:
                lines.append(line)
    if current is not None:
        yield current, lines


def hex_float(token):
    return struct.unpack("<f", struct.pack("<I", int(token, 16)))[0]


def geometry_close(java, native):
    """QUAD/BONE lines: same shape, floats within tolerance. Returns the max scaled error or None."""
    java_fields = java.split("\t")
    native_fields = native.split("\t")
    if len(java_fields) != len(native_fields) or java_fields[0] != native_fields[0]:
        return None
    worst = 0.0
    for index, (a, b) in enumerate(zip(java_fields, native_fields)):
        if a == b:
            continue
        # BONE: name, parent and cube count must match exactly.
        if java_fields[0] == "BONE" and index in (1, 2, 5):
            return None
        a_values = a.split(",")
        b_values = b.split(",")
        if len(a_values) != len(b_values):
            return None
        for x, y in zip(a_values, b_values):
            try:
                fx = hex_float(x)
                fy = hex_float(y)
            except ValueError:
                return None
            error = abs(fx - fy) / max(1.0, abs(fx), abs(fy))
            if not error <= TOLERANCE:  # also rejects NaN
                return None
            worst = max(worst, error)
    return worst


def compare_sample(java_lines, native_lines):
    is_binary = any(line.startswith("GEOMETRY\tbinary/") for line in java_lines)
    if is_binary:
        java_lines = [TEXTURE_REWRITE.sub(r"\1.encoded", line) if line.startswith("TEXTURE\t") else line
                      for line in java_lines]
    mismatches = []
    tolerated = 0
    worst = 0.0
    exact = 0
    for index in range(max(len(java_lines), len(native_lines))):
        a = java_lines[index] if index < len(java_lines) else "<missing>"
        b = native_lines[index] if index < len(native_lines) else "<missing>"
        if a == b:
            exact += 1
            continue
        if a.startswith(("QUAD\t", "BONE\t")):
            error = geometry_close(a, b)
            if error is not None:
                tolerated += 1
                worst = max(worst, error)
                continue
        mismatches.append((index, a, b))
    return len(java_lines), exact, tolerated, worst, mismatches


def main():
    failed = 0
    count = 0
    pairs = zip_longest(read_samples(sys.argv[1]), read_samples(sys.argv[2]))
    for reference, candidate in pairs:
        count += 1
        if reference is None or candidate is None or reference[0] != candidate[0]:
            print("MISSING_OR_REORDERED\t" + str((reference and reference[0], candidate and candidate[0])))
            failed += 1
            continue
        name, java_lines = reference
        _, native_lines = candidate
        if (not java_lines or not native_lines
                or any(line.startswith("ERROR\t") for line in java_lines + native_lines)):
            print(f"ERROR\t{name}\tempty or unsuccessful parse")
            failed += 1
            continue
        lines, exact, tolerated, worst, mismatches = compare_sample(java_lines, native_lines)
        if mismatches:
            failed += 1
        status = "DIFF" if mismatches else "OK"
        print(f"{status}\t{name}\tlines={lines} exact={exact} tolerated={tolerated} "
              f"max_scaled_error={worst:.3g} mismatches={len(mismatches)}")
        for index, a, b in mismatches[:4]:
            print(f"  line {index}\n    java:   {a[:300]}\n    native: {b[:300]}")
    print(f"samples={count} failed={failed}")
    return 1 if failed or count == 0 else 0


if __name__ == "__main__":
    sys.exit(main())
