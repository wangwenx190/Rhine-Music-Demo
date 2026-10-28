#!/usr/bin/env python3
"""Run serialized Release product detail replays; keep raw evidence outside Git."""
from native_replay import matching_number, replay_parser, run_replays


def validate_report(report, args, output):
    transitions = report.get("windowTransitions")
    return (matching_number(report.get("dpr"), args.dpr)
            and report.get("windowLifecycleRequested") is args.window_lifecycle
            and isinstance(transitions, list)
            and len(transitions) == (4 if args.window_lifecycle else 0)
            and all(isinstance(state, dict) for state in transitions))


def main():
    parser = replay_parser()
    parser.add_argument("--window-lifecycle", action="store_true",
                        help="Check one same-window hide and minimize restore")
    args = parser.parse_args()
    return run_replays(parser, args, "--verify-detail", validate_report,
                       windowLifecycle=args.window_lifecycle)


if __name__ == "__main__":
    raise SystemExit(main())
