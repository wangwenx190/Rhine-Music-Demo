#!/usr/bin/env python3
"""Run serialized Release product overview replays; keep raw evidence outside Git."""
from native_replay import replay_parser, run_replays


def validate_report(report, args):
    if args.visual:
        captures = report.get("captures", [])
        lifecycle = report.get("lifecycle", [])
        context_valid = (len(captures) == 36 and all(c["dpr"] == args.dpr for c in captures)
                         and len(lifecycle) == (10 if args.lifecycle else 0) and all(lifecycle))
    else:
        context_valid = report.get("dpr") == args.dpr and report.get("cycles") == (10 if args.lifecycle else 0)
    return (context_valid)


parser = replay_parser()
parser.add_argument("--visual", action="store_true", help="Run the isolated QML/resource replay")
args = parser.parse_args()
raise SystemExit(run_replays(parser, args, "--overview" if args.visual else "--verify-overview", validate_report, visual=args.visual))
