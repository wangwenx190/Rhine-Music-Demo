"""Read verification objects and reject numbers outside finite runtime arithmetic."""
import json
import math


def finite_number(value):
    if type(value) not in (int, float):
        return False
    try:
        return math.isfinite(value)
    except OverflowError:
        return False


def read_report(path):
    def reject_constant(value):
        raise ValueError(f"Nonfinite JSON number: {value}")

    def checked_number(value, convert):
        result = convert(value)
        if not finite_number(result):
            reject_constant(value)
        return result

    try:
        report = json.loads(path.read_text(encoding="utf-8"), parse_constant=reject_constant,
                            parse_float=lambda value: checked_number(value, float),
                            parse_int=lambda value: checked_number(value, int))
    except (OSError, ValueError):
        return {}
    return report if isinstance(report, dict) else {}
