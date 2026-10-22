#!/usr/bin/env python3
"""Compare same-input native and frozen logical boxes; never read or score images."""
import argparse
import json
import math
from pathlib import Path

FIELDS = {
    "presentation": {
        "article": ("x", "y", "width", "height"), "caption": ("x", "y", "height"),
        "back": ("x", "y", "fontSize"), "title": ("x", "y", "width", "height", "fontSize"),
        "artist": ("x", "y", "width"), "facts": ("x", "y", "width", "height", "gap", "rowGap"),
        "factLabel": ("x", "y", "width", "height", "fontSize"),
        "factValue": ("x", "y", "width", "height", "fontSize"),
        "tabs": ("x", "y", "width", "height"),
        "trackTitle": ("x", "y", "width", "fontSize"), "trackArtist": ("x", "y", "width", "fontSize"),
    },
    "overview": {name: ("x", "y", "width", "height")
                 for name in ("card", "label", "name", "count", "entry")},
}


def identity(record):
    width, height, night, dpr = (record[key] for key in ("width", "height", "night", "dpr"))
    if (type(width) is not int or width <= 0 or type(height) is not int or height <= 0
            or type(night) is not bool or type(dpr) not in (int, float)
            or not math.isfinite(dpr) or dpr <= 0):
        raise ValueError("Invalid viewport/theme/DPR identity")
    return width, height, night, dpr


def indexed(records, key):
    if not isinstance(records, list) or not records:
        raise ValueError("Nonempty structure records required")
    result = {}
    for record in records:
        token = key(record)
        if token in result:
            raise ValueError("Duplicate structure state")
        result[token] = record
    return result


def box_checks(actual, expected, fields, state):
    checks = []
    for part, properties in fields.items():
        for name in properties:
            a, b = actual[part][name], expected[part][name]
            if (type(a) not in (int, float) or type(b) not in (int, float)
                    or not math.isfinite(a) or not math.isfinite(b)):
                raise ValueError("Finite numeric geometry required")
            delta = abs(a - b)
            checks.append(dict(state=state, part=part, field=name, delta=delta, passed=delta <= 1))
    return checks


def compare(component, native, source):
    if not isinstance(native, dict) or not isinstance(source, dict):
        raise ValueError("Native and source reports must be objects")
    if native.get("passed") is not True or native.get("qmlWarnings"):
        raise ValueError("Passing native report without QML warnings required")
    if component == "overview" and native.get("retired") is not True:
        raise ValueError("Completed overview retirement required")
    captures = native["captures"]
    references = indexed(source["captures"], identity)
    dprs = {identity(c)[-1] for c in captures}
    expected = {key for key in references if key[-1] in dprs}
    checks = []
    if component == "overview":
        def admission_key(capture):
            expanded = capture["expanded"]
            if type(expanded) is not int or expanded < -1:
                raise ValueError("Invalid expanded lane")
            return (*identity(capture), expanded)

        actual = indexed(captures, admission_key)
        admissions = indexed([c for c in source["admissions"]
                              if c["backend"] == native["backend"] and identity(c)[-1] in dprs],
                             admission_key)
        if set(actual) != set(admissions) or {key[:4] for key in actual} != expected:
            raise ValueError("Exactly one matching native/source record per declared state required")
        for key, capture in actual.items():
            cards = capture["structures"]
            lanes = [card["lane"] for card in cards]
            expected_lanes = admissions[key]["lanes"]
            if (not isinstance(expected_lanes, list)
                    or any(type(lane) is not int or lane < 0 for lane in lanes + expected_lanes)):
                raise ValueError("Nonnegative integer lanes required")
            if len(set(lanes)) != len(lanes) or sorted(lanes) != sorted(expected_lanes):
                raise ValueError("Frozen label admission mismatch")
            variants = indexed(references[key[:4]]["variants"], lambda v: v["expanded"])
            if set(variants) != {False, True} or any(type(v) is not bool for v in variants):
                raise ValueError("Collapsed and expanded source variants required")
            for card in cards:
                if (type(card["expanded"]) is not bool
                        or card["expanded"] != (card["lane"] == key[-1])):
                    raise ValueError("Card expansion must match the declared expanded lane")
                checks.extend(box_checks(card, variants[card["expanded"]], FIELDS[component], key))
    else:
        actual = indexed(captures, identity)
        if set(actual) != expected:
            raise ValueError("Exactly one matching native/source record per declared state required")
        for key, capture in actual.items():
            reference = references[key]
            checks.extend(box_checks(capture["fields"], reference["fields"], FIELDS[component], key))
    if not checks:
        raise ValueError("No logical boxes compared")
    return dict(passed=all(c["passed"] for c in checks), states=len(captures),
                toleranceLogicalPx=1, maximumLogicalPixelError=max(c["delta"] for c in checks),
                checks=checks, scope="Frozen CSS/DOM logical boxes; no image comparison")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("component", choices=FIELDS)
    parser.add_argument("native", type=Path)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Choose a fresh output file; preserve prior results")
    try:
        result = compare(args.component, json.loads(args.native.read_text(encoding="utf-8")),
                         json.loads(args.source.read_text(encoding="utf-8")))
    except (OSError, ValueError, TypeError, KeyError, IndexError) as error:
        result = dict(passed=False, error=str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"{args.component} structure: passed={result['passed']}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
