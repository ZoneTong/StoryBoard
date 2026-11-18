#!/usr/bin/env python3
"""对账 src 里的翻译调用与 translations/storyboard_zh.ts 的条目。

两类调用都要覆盖：
  * uiText("English")        —— 手工维护的 StoryBoard 上下文条目（无 location）
  * tr("English") / X::tr()   —— 各 Qt 类上下文条目（lupdate 常规产物）

ts 是手工维护的（见项目约定：不直接跑 lupdate），所以这里只做只读对账：
  HARD 缺失：源码里的串在 ts 里任何上下文都找不到
  SOFT 偏差：串存在，但不在调用点应有的上下文里

用法： python scripts/check-translations.py [--quiet]
返回码：0 无 HARD 缺失；1 有 HARD 缺失。
"""

from __future__ import annotations

import argparse
import html
import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src"
TS = ROOT / "translations" / "storyboard_zh.ts"
CALL_RE = re.compile(r"(?:([A-Za-z_][A-Za-z0-9_]*)\s*::\s*)?(uiText|tr|translate)\s*\(")
LITERAL_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')
# 只认行首的成员定义（`void CanvasScene::foo()`），避免把局部变量调用当成所属类。
CLASS_DEF_RE = re.compile(r"^[\w:<>,*&\s]*?\b([A-Z][A-Za-z0-9_]*)::[A-Za-z_~]", re.M)


def unescape(text: str) -> str:
    return (
        text.replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace('\\"', '"')
        .replace("\\\\", "\\")
    )


def ts_source(text: str) -> str:
    """ts 里的串是 XML 文本：&amp; / &apos; / &quot; / &lt; 先还原成真实字符。"""
    return html.unescape(text)


def call_arguments(source: str, open_paren: int) -> str:
    """返回从 open_paren（指向 '('）起配平的括号内文本。"""
    depth = 0
    for index in range(open_paren, len(source)):
        char = source[index]
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return source[open_paren + 1 : index]
        elif char == '"':
            # 跳过字符串字面量，避免其中的括号干扰配平。
            index += 1
            while index < len(source):
                if source[index] == "\\":
                    index += 2
                    continue
                if source[index] == '"':
                    break
                index += 1
    return source[open_paren + 1 :]


def dominant_context(path: Path, source: str) -> str | None:
    """按文件里的成员定义推断所属类（拆分的 CanvasSceneXxx.cpp 都实现 CanvasScene）。"""
    skip = {"QString", "QPointF", "QSizeF", "QColor", "QPainterPath", "QJsonObject", "QRectF"}
    counts: dict[str, int] = defaultdict(int)
    for match in CLASS_DEF_RE.finditer(source):
        name = match.group(1)
        if name not in skip:
            counts[name] += 1
    if not counts:
        return None
    best = max(counts.items(), key=lambda item: item[1])
    return best[0]


def parse_ts() -> set[tuple[str, str]]:
    raw = TS.read_text(encoding="utf-8")
    pairs: set[tuple[str, str]] = set()
    for context_block in re.findall(r"<context>(.*?)</context>", raw, re.S):
        name_match = re.search(r"<name>(.*?)</name>", context_block, re.S)
        context = name_match.group(1).strip() if name_match else ""
        for message in re.findall(r"<message>(.*?)</message>", context_block, re.S):
            source_match = re.search(r"<source>(.*?)</source>", message, re.S)
            if not source_match:
                continue
            pairs.add((context, ts_source(unescape(source_match.group(1)))))
    return pairs


def code_calls() -> list[tuple[str, int, str, str]]:
    calls: list[tuple[str, int, str, str]] = []
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in {".cpp", ".h"} or "selftest" in path.parts:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        fallback = dominant_context(path, text)
        for match in CALL_RE.finditer(text):
            qualifier, kind = match.group(1), match.group(2)
            args = call_arguments(text, match.end() - 1)
            literals = LITERAL_RE.findall(args)
            if not literals:
                continue
            if kind == "translate":
                # translate("上下文", "源串")：第一个字面量是上下文，不是待翻译文本。
                if len(literals) < 2:
                    continue
                context = unescape(literals[0])
                source = unescape("".join(literals[1:]))
            else:
                source = unescape("".join(literals))
                if qualifier:
                    context = qualifier
                elif kind == "uiText":
                    context = "StoryBoard"
                else:
                    context = fallback or "?"
            line = text.count("\n", 0, match.start()) + 1
            calls.append((str(path.relative_to(ROOT)), line, context, source))
    return calls


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--quiet", action="store_true", help="只打印汇总")
    parser.add_argument("--root", help="仓库根目录（默认取脚本所在仓库，便于跨工作树对账）")
    args = parser.parse_args()
    global ROOT, SRC, TS
    if args.root:
        ROOT = Path(args.root).resolve()
        SRC = ROOT / "src"
        TS = ROOT / "translations" / "storyboard_zh.ts"

    known = parse_ts()
    sources_by_context: dict[str, set[str]] = defaultdict(set)
    all_sources: set[str] = set()
    for context, source in known:
        sources_by_context[context].add(source)
        all_sources.add(source)

    calls = code_calls()
    hard: list[tuple[str, int, str, str]] = []
    soft: list[tuple[str, int, str, str]] = []
    for call in calls:
        context, source = call[2], call[3]
        if (context, source) in known:
            continue
        if source not in all_sources:
            hard.append(call)
        else:
            soft.append(call)

    unique_hard = sorted({(call[2], call[3]) for call in hard})
    print(f"ts 条目: {len(known)}  (上下文 {len(sources_by_context)})")
    print(f"源码调用: {len(calls)}  唯一串: {len({(c[2], c[3]) for c in calls})}")
    print(f"HARD 缺失(全表找不到): {len(unique_hard)}")
    if hard and not args.quiet:
        for path, line, context, source in hard:
            print(f"  {path}:{line}  [{context}] {source!r}")
    print(f"SOFT 偏差(上下文不同): {len({(c[2], c[3]) for c in soft})}")
    if soft and not args.quiet:
        for path, line, context, source in soft:
            print(f"  {path}:{line}  [{context}] {source!r}")
    return 1 if unique_hard else 0


if __name__ == "__main__":
    sys.exit(main())
