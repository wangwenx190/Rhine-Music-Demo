#!/usr/bin/env python3
"""Run serialized Release product overview replays; keep raw evidence outside Git."""
from collections import Counter
from pathlib import Path
from native_replay import VIEWPORTS, matching_number, replay_parser, run_replays


def validate_report(report, args, output):
    if not args.visual:
        return matching_number(report.get("dpr"), args.dpr)
    captures = report.get("captures")
    if not isinstance(captures, list) or not captures:
        return False
    for capture in captures:
        if (not isinstance(capture, dict)
                or any(type(capture.get(key)) is not int for key in ("width", "height", "expanded", "variant"))
                or capture["expanded"] < -1
                or (capture["variant"] == 0 and capture["expanded"] != -1)
                or type(capture.get("night")) is not bool
                or not matching_number(capture.get("dpr"), args.dpr)
                or not isinstance(capture.get("file"), str)):
            return False
    expected = Counter({(w, h, night, variant): 1 for w, h in VIEWPORTS
                        for night in (False, True) for variant in (0, 1)})
    actual = Counter((c["width"], c["height"], c["night"], c["variant"]) for c in captures)
    root = output.resolve()
    images = [(root / c["file"]).resolve() for c in captures]
    return (actual == expected and len(set(images)) == len(captures)
            and all(not Path(c["file"]).is_absolute() for c in captures)
            and all(p.is_relative_to(root) and p.is_file() and p.stat().st_size > 0 for p in images))


def main():
    parser = replay_parser()
    parser.add_argument("--visual", action="store_true", help="Run the isolated QML/resource replay")
    args = parser.parse_args()
    if args.visual and args.night:
        parser.error("The visual replay already checks both themes; --night applies to the product replay")
    return run_replays(parser, args, "--overview" if args.visual else "--verify-overview",
                       validate_report, visual=args.visual)


if __name__ == "__main__":
    raise SystemExit(main())
