"""One-off migration helper: turn the old built-in dictionary into a Qt .ts file."""
import re
import sys
from pathlib import Path
from xml.sax.saxutils import escape

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "I18n.cpp"
TARGET = ROOT / "translations" / "storyboard_en.ts"

PAIR = re.compile(
    r'\{\s*QStringLiteral\("((?:[^"\\]|\\.)*)"\),\s*QStringLiteral\("((?:[^"\\]|\\.)*)"\)\s*\}'
)


def unescape(text: str) -> str:
    return (
        text.replace('\\"', '"')
        .replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace("\\\\", "\\")
    )


def main() -> int:
    content = SOURCE.read_text(encoding="utf-8")
    pairs = []
    seen = set()
    for source, translation in PAIR.findall(content):
        source = unescape(source)
        translation = unescape(translation)
        if source in seen:
            continue
        seen.add(source)
        pairs.append((source, translation))

    if not pairs:
        print("no dictionary entries found", file=sys.stderr)
        return 1

    lines = [
        '<?xml version="1.0" encoding="utf-8"?>',
        "<!DOCTYPE TS>",
        '<TS version="2.1" language="en_US">',
        "<context>",
        "    <name>StoryBoard</name>",
    ]
    for source, translation in pairs:
        lines.append("    <message>")
        lines.append(f"        <source>{escape(source)}</source>")
        lines.append(f"        <translation>{escape(translation)}</translation>")
        lines.append("    </message>")
    lines.extend(["</context>", "</TS>", ""])

    TARGET.parent.mkdir(parents=True, exist_ok=True)
    TARGET.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {len(pairs)} messages to {TARGET}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
