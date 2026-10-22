#!/usr/bin/env python3
"""Run serialized Release product detail replays; keep raw evidence outside Git."""
from native_replay import replay_parser, run_replays


def validate_report(report, args):
    return (report.get("dpr") == args.dpr
             and report.get("lifecycleStressRequested") == args.lifecycle
             and report.get("windowLifecycleRequested") == args.window_lifecycle
             and len(report.get("windowTransitions", [])) == (4 if args.window_lifecycle else 0)
             and report.get("fullWindowRebuilds") == (10 if args.lifecycle else 0))


parser = replay_parser()
parser.add_argument("--window-lifecycle", action="store_true",
                    help="Check one same-window hide and minimize restore without stress")
args = parser.parse_args()
if args.lifecycle and args.window_lifecycle:
    parser.error("window-lifecycle and lifecycle stress are separate checks")
raise SystemExit(run_replays(parser, args, "--verify-detail", validate_report, windowLifecycle=args.window_lifecycle))
