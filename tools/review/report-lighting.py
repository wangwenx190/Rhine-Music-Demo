"""Retired historical source-image report; reject before reading or writing evidence."""
raise SystemExit(
    "Historical image acceptance reports are retired. Use tools/audit/audit-material-review.py "
    "and inspect own output; see docs/contracts/source-fidelity-review.md."
)
