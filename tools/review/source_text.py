"""Match source fragments across layout changes without changing literal contents."""

import re


class SourceText(str):
    @staticmethod
    def pattern(fragment):
        # Keep complete string literals atomic; ignore whitespace between tokens.
        tokens = re.findall(
            r"'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"|[A-Za-z_$][\w$]*|\d+|[^\s]",
            fragment,
        )
        parts = []
        for index, token in enumerate(tokens):
            if token in (')', ']', '}') and index and tokens[index - 1] != ',':
                parts.append(r",?\s*")
            parts.append(re.escape(token))
        return re.compile(r"\s*".join(parts))

    def replace(self, old, new, count=-1):
        if count == 0:
            return self
        if old in self:
            return SourceText(super().replace(old, new, count))
        return SourceText(self.pattern(old).sub(lambda match: new, self, count=max(count, 0)))

    def index(self, fragment, start=0, end=None):
        try:
            return super().index(fragment, start, len(self) if end is None else end)
        except ValueError:
            match = self.pattern(fragment).search(self, start, len(self) if end is None else end)
            if not match:
                raise ValueError(f"Source fragment missing: {fragment}") from None
            return match.start()

    def after(self, fragment):
        if fragment in self:
            return super().index(fragment) + len(fragment)
        match = self.pattern(fragment).search(self)
        if not match:
            raise ValueError(f"Source fragment missing: {fragment}")
        return match.end()

    def __getitem__(self, key):
        result = super().__getitem__(key)
        return SourceText(result) if isinstance(key, slice) else result

    def __add__(self, other):
        return SourceText(super().__add__(other))

    def __radd__(self, other):
        return SourceText(str(other) + str(self))
