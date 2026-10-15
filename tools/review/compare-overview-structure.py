"""Compare logical card/font boxes against frozen production CSS, without image scores."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("native", type=Path)
parser.add_argument("source", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
native = json.loads(args.native.read_text())
source = json.loads(args.source.read_text())
if not native.get("passed") or not native.get("retired") or native.get("qmlWarnings"):
    raise SystemExit("Passing native geometry, retirement and warning checks required")
references = {(c["width"], c["height"], c["night"], c["dpr"]): c
              for c in source["captures"]}
checks = []
admissions = {(c["width"], c["height"], c["night"], c["dpr"], c["expanded"], c["backend"]): c["lanes"]
              for c in source["admissions"]}
for capture in native["captures"]:
    key = tuple(capture[k] for k in ("width", "height", "night", "dpr"))
    reference = references[key]
    expected_lanes = admissions[(*key, capture["expanded"], native["backend"])]
    actual_lanes = [c["lane"] for c in capture["structures"]]
    if sorted(actual_lanes) != sorted(expected_lanes):
        raise SystemExit(f"Frozen label admission mismatch: {key}: {actual_lanes} vs {expected_lanes}")
    for card in capture["structures"]:
        expected = next(v for v in reference["variants"] if v["expanded"] == card["expanded"])
        for part in ("card", "label", "name", "count", "entry"):
            for field in ("x", "y", "width", "height"):
                delta = abs(card[part][field] - expected[part][field])
                checks.append(dict(size=key, lane=card["lane"], expanded=card["expanded"],
                                   part=part, field=field, delta=delta, passed=delta <= 1))
result = dict(passed=len(native["captures"]) == 36 and all(c["passed"] for c in checks),
              scope="Frozen CSS/DOM logical boxes; no source image/GPU comparison",
              maximumLogicalPixelError=max(c["delta"] for c in checks), checks=checks)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
for check in checks:
    if not check["passed"]:
        print(check)
print(f"Overview structure: {'passed' if result['passed'] else 'FAILED'}, "
      f"maximum {result['maximumLogicalPixelError']:g} logical px")
raise SystemExit(0 if result["passed"] else 1)
